# QtScrcpy 改进计划

本文档记录了 QtScrcpy 项目的完整改进方案，包括问题分析、解决方案和实施步骤。

## 📋 改进概览

### 已发现的问题
1. ✅ 文件传输无完成提示
2. ✅ 截图无保存路径提示
3. ✅ 手机端文件路径不可配置
4. ✅ 上传图片后相册不刷新
5. ⚠️ 缺少手机设置助手

---

## 🎯 阶段 1：紧急修复（优先级：高）

### 1.1 添加文件传输和截图完成通知

**问题分析：**
- 核心代码已经发出信号（`FileHandler::fileHandlerResult`）
- 但 UI 层没有监听和显示

**解决方案：使用系统托盘通知**

#### 修改文件 1：`QtScrcpy/ui/dialog.cpp`

在 `onDeviceConnected` 函数中添加信号连接：

```cpp
void Dialog::onDeviceConnected(bool success, const QString& serial, const QString& deviceName, const QSize& size)
{
    // ... 现有代码 ...

    auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
    if (!device) {
        return;
    }

    // 监听文件传输结果
    connect(device, &qsc::IDevice::fileTransferResult, this,
        [this](bool success, bool isApk, const QString& message) {
            if (m_hideIcon) {
                QSystemTrayIcon::MessageIcon icon = success ?
                    QSystemTrayIcon::Information : QSystemTrayIcon::Warning;
                QString title = isApk ? tr("APK Installation") : tr("File Transfer");
                m_hideIcon->showMessage(title, message, icon, 3000);
            }
        });

    // 监听截图结果
    connect(device, &qsc::IDevice::screenshotResult, this,
        [this](bool success, const QString& filePath) {
            if (m_hideIcon) {
                if (success) {
                    m_hideIcon->showMessage(
                        tr("Screenshot"),
                        tr("Saved to:\n%1").arg(filePath),
                        QSystemTrayIcon::Information,
                        3000
                    );
                } else {
                    m_hideIcon->showMessage(
                        tr("Screenshot Failed"),
                        tr("Please set record path first in Start Config"),
                        QSystemTrayIcon::Warning,
                        3000
                    );
                }
            }
        });
}
```

#### 修改文件 2：`QtScrcpy/QtScrcpyCore/include/QtScrcpyCore.h`

在 `IDevice` 类的 `signals:` 部分添加：

```cpp
signals:
    void deviceConnected(bool success, const QString& serial, const QString& deviceName, const QSize& size);
    void deviceDisconnected(QString serial);

    // 新增信号
    void fileTransferResult(bool success, bool isApk, const QString& message);
    void screenshotResult(bool success, const QString& filePath);
```

#### 修改文件 3：`QtScrcpy/QtScrcpyCore/src/device/device.cpp`

**位置 1：修改 `initSignals()` 函数（第 148-167 行）**

```cpp
if (m_fileHandler) {
    connect(m_fileHandler, &FileHandler::fileHandlerResult, this,
        [this](FileHandler::FILE_HANDLER_RESULT processResult, bool isApk) {
            QString tipsType = isApk ? "install apk" : "file transfer";
            QString tips;
            bool success = false;

            if (FileHandler::FAR_IS_RUNNING == processResult) {
                tips = QString("wait current %1 to complete").arg(tipsType);
            }
            if (FileHandler::FAR_SUCCESS_EXEC == processResult) {
                tips = QString("%1 complete, save in %2").arg(tipsType).arg(m_params.pushFilePath);
                success = true;
            }
            if (FileHandler::FAR_ERROR_EXEC == processResult) {
                tips = QString("%1 failed").arg(tipsType);
                success = false;
            }
            qInfo() << tips;

            // 发出信号通知 UI
            emit fileTransferResult(success, isApk, tips);
        });
}
```

**位置 2：修改 `saveFrame()` 函数（第 606-636 行）**

```cpp
bool Device::saveFrame(int width, int height, uint8_t* dataRGB32)
{
    if (!dataRGB32) {
        emit screenshotResult(false, "");
        return false;
    }

    QImage rgbImage(dataRGB32, width, height, QImage::Format_RGB32);

    QString absFilePath;
    QString fileDir(m_params.recordPath);
    if (fileDir.isEmpty()) {
        qWarning() << "please select record save path!!!";
        emit screenshotResult(false, "");
        return false;
    }

    QDateTime dateTime = QDateTime::currentDateTime();
    QString fileName = dateTime.toString("_yyyyMMdd_hhmmss_zzz");
    fileName = m_params.serial + fileName;
    fileName.replace(":", "_");
    fileName.replace(".", "_");
    fileName += ".png";

    QDir dir(fileDir);
    absFilePath = dir.absoluteFilePath(fileName);
    int ret = rgbImage.save(absFilePath, "PNG", 100);

    if (!ret) {
        emit screenshotResult(false, absFilePath);
        return false;
    }

    qInfo() << "screenshot save to " << absFilePath;
    emit screenshotResult(true, absFilePath);
    return true;
}
```

---

### 1.2 解决相册刷新问题

**问题原因：**
Android 系统不会自动扫描通过 ADB 推送的文件，需要手动触发媒体扫描。

**解决方案：**

#### 修改文件：`QtScrcpy/QtScrcpyCore/src/device/filehandler/filehandler.cpp`

```cpp
void FileHandler::onPushFileRequest(const QString &serial, const QString &file, const QString &devicePath)
{
    qsc::AdbProcess* adb = new qsc::AdbProcess;
    bool isApk = false;

    // 保存设备路径，用于后续触发媒体扫描
    QString savedDevicePath = devicePath;

    connect(adb, &qsc::AdbProcess::adbProcessResult, this,
        [this, adb, isApk, serial, savedDevicePath](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
            onAdbProcessResult(adb, isApk, processResult);

            // 如果推送成功，触发媒体扫描
            if (processResult == qsc::AdbProcess::AER_SUCCESS_EXEC) {
                triggerMediaScan(serial, savedDevicePath);
            }
        });

    adb->push(serial, file, devicePath);
}

// 新增函数：触发媒体扫描
void FileHandler::triggerMediaScan(const QString &serial, const QString &filePath)
{
    qsc::AdbProcess* adb = new qsc::AdbProcess;
    connect(adb, &qsc::AdbProcess::adbProcessResult, this,
        [adb](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
            if (processResult != qsc::AdbProcess::AER_SUCCESS_START) {
                adb->deleteLater();
            }
        });

    // 构造媒体扫描命令
    QStringList args;
    args << "shell" << "am" << "broadcast"
         << "-a" << "android.intent.action.MEDIA_SCANNER_SCAN_FILE"
         << "-d" << QString("file://%1").arg(filePath);

    adb->execute(serial, args);

    qInfo() << "Triggered media scan for:" << filePath;
}
```

#### 修改文件：`QtScrcpy/QtScrcpyCore/src/device/filehandler/filehandler.h`

```cpp
class FileHandler : public QObject
{
    Q_OBJECT
public:
    // ... 现有代码 ...

protected:
    void onAdbProcessResult(qsc::AdbProcess* adb, bool isApk, qsc::AdbProcess::ADB_EXEC_RESULT processResult);
    void triggerMediaScan(const QString &serial, const QString &filePath);  // 新增

signals:
    void fileHandlerResult(FILE_HANDLER_RESULT processResult, bool isApk = false);
};
```

---

## 🎯 阶段 2：功能完善（优先级：中）

### 2.1 添加路径配置界面

#### 修改文件 1：`QtScrcpy/util/config.h`

```cpp
class Config : public QObject
{
    // ... 现有代码 ...

public:
    // 新增函数
    QString getScreenshotPath();
    void setScreenshotPath(const QString &path);
    void setPushFilePath(const QString &path);
};
```

#### 修改文件 2：`QtScrcpy/util/config.cpp`

```cpp
#include <QStandardPaths>

#define COMMON_SCREENSHOT_PATH_KEY "ScreenshotPath"
#define COMMON_SCREENSHOT_PATH_DEF ""

QString Config::getScreenshotPath()
{
    QString path;
    m_userData->beginGroup(GROUP_COMMON);
    path = m_userData->value(COMMON_SCREENSHOT_PATH_KEY, COMMON_SCREENSHOT_PATH_DEF).toString();
    m_userData->endGroup();

    if (path.isEmpty()) {
        // 默认使用系统图片文件夹
        path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    return path;
}

void Config::setScreenshotPath(const QString &path)
{
    m_userData->beginGroup(GROUP_COMMON);
    m_userData->setValue(COMMON_SCREENSHOT_PATH_KEY, path);
    m_userData->endGroup();
    m_userData->sync();
}

void Config::setPushFilePath(const QString &path)
{
    m_settings->beginGroup(GROUP_COMMON);
    m_settings->setValue(COMMON_PUSHFILE_KEY, path);
    m_settings->endGroup();
    m_settings->sync();
}
```

#### 修改文件 3：在 `dialog.ui` 中添加配置项

在"启动配置"对话框中添加：

1. **手机端文件路径**
   - Label: "手机端保存路径："
   - LineEdit: 显示和编辑路径（默认 /sdcard/）
   - 说明: "文件将保存到手机的此路径"

2. **电脑端截图路径**
   - Label: "截图保存路径："
   - LineEdit + Button: 选择文件夹
   - 说明: "截图将保存到电脑的此路径"

---

## 🎯 阶段 3：手机设置助手（优先级：中）

### 3.1 添加设置检测和引导

#### 新增文件：`QtScrcpy/ui/devicesetupwizard.h`

```cpp
#ifndef DEVICESETUPWIZARD_H
#define DEVICESETUPWIZARD_H

#include <QDialog>

namespace Ui {
class DeviceSetupWizard;
}

class DeviceSetupWizard : public QDialog
{
    Q_OBJECT

public:
    explicit DeviceSetupWizard(const QString &serial, QWidget *parent = nullptr);
    ~DeviceSetupWizard();

private slots:
    void checkUsbDebugging();
    void checkMockLocation();
    void openDeveloperSettings();
    void openSecuritySettings();

private:
    Ui::DeviceSetupWizard *ui;
    QString m_serial;
};

#endif
```

#### 功能列表：

1. **USB 调试检测**
   - 检测是否开启 USB 调试
   - 提供开启步骤引导

2. **模拟点击权限检测**
   - 检测是否允许模拟点击
   - 提供开启步骤引导（小米等手机）

3. **快速设置按钮**
   - 一键打开开发者选项
   - 一键打开安全设置

4. **常见问题修复**
   - 重启 ADB 服务
   - 检查 ADB 版本冲突
   - 测试连接状态

---

## 📝 实施优先级总结

### 🔥 立即实施（今天）
1. ✅ 添加文件传输完成通知（30分钟）
2. ✅ 添加截图完成通知（10分钟）
3. ✅ 解决相册刷新问题（20分钟）

**总计：1小时**

### 📅 本周实施
4. ⏰ 添加路径配置界面（2小时）
5. ⏰ 完善配置文件说明（30分钟）

### 📅 下周实施
6. 🔮 添加手机设置助手（4-6小时）
7. 🔮 添加设置检测功能（2-3小时）

---

## 🧪 测试计划

### 测试 1：文件传输通知
1. 拖拽图片到窗口
2. 检查是否显示托盘通知
3. 检查手机相册是否立即显示图片

### 测试 2：截图通知
1. 点击截图按钮
2. 检查是否显示托盘通知
3. 检查通知中的路径是否正确

### 测试 3：相册刷新
1. 传输图片到手机
2. 立即打开手机相册
3. 确认图片已显示（无需重启）

---

## 📚 参考资料

- Android MediaScanner: https://developer.android.com/reference/android/media/MediaScannerConnection
- ADB 命令参考: https://developer.android.com/studio/command-line/adb
- Qt 系统托盘: https://doc.qt.io/qt-5/qsystemtrayicon.html
