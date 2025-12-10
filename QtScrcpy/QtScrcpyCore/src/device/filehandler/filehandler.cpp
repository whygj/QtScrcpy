#include "filehandler.h"

FileHandler::FileHandler(QObject *parent) : QObject(parent)
{
}

FileHandler::~FileHandler() {}

void FileHandler::onPushFileRequest(const QString &serial, const QString &file, const QString &devicePath)
{
    qsc::AdbProcess* adb = new qsc::AdbProcess;
    bool isApk = false;

    // 保存设备路径，用于后续触发媒体扫描
    QString savedDevicePath = devicePath;

    connect(adb, &qsc::AdbProcess::adbProcessResult, this, [this, adb, isApk, serial, savedDevicePath](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
        onAdbProcessResult(adb, isApk, processResult);

        // 如果推送成功，触发媒体扫描（解决相册不刷新问题）
        if (processResult == qsc::AdbProcess::AER_SUCCESS_EXEC) {
            triggerMediaScan(serial, savedDevicePath);
        }
    });

    adb->push(serial, file, devicePath);
}

void FileHandler::onInstallApkRequest(const QString &serial, const QString &apkFile)
{
    qsc::AdbProcess* adb = new qsc::AdbProcess;
    bool isApk = true;
    connect(adb, &qsc::AdbProcess::adbProcessResult, this, [this, adb, isApk](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
        onAdbProcessResult(adb, isApk, processResult);
    });

    adb->install(serial, apkFile);
}

void FileHandler::onAdbProcessResult(qsc::AdbProcess *adb, bool isApk, qsc::AdbProcess::ADB_EXEC_RESULT processResult)
{
    switch (processResult) {
    case qsc::AdbProcess::AER_ERROR_START:
    case qsc::AdbProcess::AER_ERROR_EXEC:
    case qsc::AdbProcess::AER_ERROR_MISSING_BINARY:
        emit fileHandlerResult(FAR_ERROR_EXEC, isApk);
        adb->deleteLater();
        break;
    case qsc::AdbProcess::AER_SUCCESS_EXEC:
        emit fileHandlerResult(FAR_SUCCESS_EXEC, isApk);
        adb->deleteLater();
        break;
    default:
        break;
    }
}

void FileHandler::triggerMediaScan(const QString &serial, const QString &filePath)
{
    qsc::AdbProcess* adb = new qsc::AdbProcess;
    connect(adb, &qsc::AdbProcess::adbProcessResult, this, [adb](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
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
