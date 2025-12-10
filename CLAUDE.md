# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

QtScrcpy 是一个基于 Qt 和 C++ 的 Android 设备屏幕镜像和控制应用程序，支持 Windows、macOS 和 Linux 三大平台。它是对 Genymobile 的 scrcpy 项目的重新实现，使用 Qt 作为 UI 框架，OpenGL 进行视频渲染，FFmpeg 进行视频解码。

**核心特性：**
- 通过 USB 或 WiFi 显示和控制 Android 设备
- 低延迟（35-70ms）、高性能（30-60fps）
- 支持自定义键盘映射（用于游戏控制）
- 支持群控（同时控制多个设备）
- 支持音频转发（基于 sndcpy，Android 10+）

## 构建命令

### Windows
```bash
# 需要设置环境变量 ENV_QT_PATH（例如：D:\Qt\Qt5.12.5\5.12.5）
cd ci/win
build_for_win.bat [Debug|Release|MinSizeRel|RelWithDebInfo] [x86|x64]

# 示例
build_for_win.bat Release x64
```

### macOS
```bash
# 需要设置环境变量 ENV_QT_PATH（例如：/Users/barry/Qt5.12.5/5.12.5）
cd ci/mac
./build_for_mac.sh [Debug|Release|MinSizeRel|RelWithDebInfo] [x64|arm64]

# 示例
./build_for_mac.sh Release arm64
```

### Linux
```bash
# 需要设置环境变量 ENV_QT_PATH（例如：/home/barry/Qt5.9.6/5.9.6）
cd ci/linux
./build_for_linux.sh [Debug|Release|MinSizeRel|RelWithDebInfo]

# 示例
./build_for_linux.sh Release
```

**构建输出位置：** `output/<架构>/<构建类型>/`

### 翻译文件更新
```bash
# 更新翻译源文件（.ts）
./ci/lupdate.sh

# 编译翻译文件（.ts -> .qm）
./ci/lrelease.sh
```

### 代码格式化
```bash
cd QtScrcpy
./clang-format-all.sh
```

## 项目架构

### 客户端-服务器架构

QtScrcpy 采用客户端-服务器架构：

**服务器端（scrcpy-server）：**
- 运行在 Android 设备上的 Java 应用（APK）
- 使用 MediaCodec API 进行 H.264 视频编码
- 通过反射调用 Android 隐藏 API 进行屏幕捕获和输入注入
- 位置：`QtScrcpy/QtScrcpyCore/src/third_party/scrcpy-server`

**客户端（QtScrcpy）：**
- 运行在主机上的 Qt/C++ 应用
- 负责推送服务器到设备并启动
- 使用 FFmpeg 解码 H.264 视频流
- 使用 OpenGL 渲染视频帧
- 处理键盘/鼠标事件并转换为 Android 触摸事件

### 目录结构

```
QtScrcpy/
├── QtScrcpy/              # 主应用程序代码
│   ├── ui/                # Qt UI 组件（Dialog, VideoForm, ToolForm）
│   ├── render/            # OpenGL 视频渲染（qyuvopenglwidget）
│   ├── audio/             # 音频输出处理
│   ├── groupcontroller/   # 群控功能
│   ├── util/              # 工具类（配置、鼠标点击模拟）
│   ├── uibase/            # 基础 UI 组件（保持比例的 widget、磁性 widget）
│   ├── fontawesome/       # 图标助手
│   ├── QtScrcpyCore/      # 核心功能库（ADB 通信、视频解码、控制逻辑）
│   └── main.cpp           # 应用程序入口
├── keymap/                # 键盘映射脚本（JSON 格式）
├── config/                # 配置文件
├── ci/                    # 构建脚本（Windows/Mac/Linux）
└── docs/                  # 文档
```

### 核心组件交互

**视频流处理：**
1. 服务器通过 socket 发送 H.264 视频流
2. Stream 线程接收数据
3. Decoder（FFmpeg）解码视频帧
4. OpenGL 渲染器在主线程渲染帧

**控制消息流：**
1. 主线程捕获 SDL/Qt 事件
2. InputManager 将事件转换为 Android 控制消息
3. Controller 线程将消息序列化并发送到设备
4. 服务器通过 InputManager.injectInputEvent 注入事件

### 线程模型

**客户端使用 4 个主要线程：**
- **主线程：** SDL/Qt 事件循环和渲染
- **Stream 线程：** 接收视频、解码和录制
- **Controller 线程：** 发送控制消息到设备
- **Receiver 线程：** 接收设备消息（剪贴板等）

## 键盘映射系统

键盘映射文件位于 `keymap/` 目录，使用 JSON 格式。坐标使用相对位置（0.0-1.0），键码使用 Qt 枚举。

**映射类型：**
- `switchKey`: 切换自定义映射的按键
- `mouseMoveMap`: 鼠标移动映射（用于 FPS 游戏视角控制）
- `keyMapNodes`: 通用按键映射数组
  - `KMT_CLICK`: 普通点击
  - `KMT_CLICK_TWICE`: 双击
  - `KMT_CLICK_MULTI`: 多点点击
  - `KMT_DRAG`: 拖拽
  - `KMT_STEER_WHEEL`: 方向盘映射（FPS 游戏移动）

详细说明参见 [docs/KeyMapDes.md](docs/KeyMapDes.md)

## 技术栈

- **UI 框架：** Qt 5.12+ 或 Qt 6
- **视频编码：** H.264（Android MediaCodec）
- **视频解码：** FFmpeg
- **视频渲染：** OpenGL
- **构建系统：** CMake 3.19+
- **编译器：** MSVC 2019+（Windows）、GCC/Clang（Linux/Mac）
- **Android 通信：** ADB
- **最低 Android 版本：** API 21（Android 5.0）

## 开发注意事项

### 代码风格
- 使用 `.clang-format` 配置进行代码格式化
- 运行 `QtScrcpy/clang-format-all.sh` 格式化所有代码
- C++ 标准：C++11
- 编译器警告视为错误（`-Werror` / `/WX`）

### 平台特定代码
- Windows：使用 `winmousetap` 进行鼠标模拟
- Linux：使用 `xmousetap`（依赖 xcb）
- macOS：使用 `cocoamousetap`（Objective-C++）

### 环境变量
应用程序使用以下环境变量定位资源：
- `QTSCRCPY_ADB_PATH`: ADB 可执行文件路径
- `QTSCRCPY_SERVER_PATH`: scrcpy-server 路径
- `QTSCRCPY_KEYMAP_PATH`: 键盘映射目录
- `QTSCRCPY_CONFIG_PATH`: 配置文件目录

### 日志配置
使用 CMake 选项 `-DENABLE_DETAILED_LOGS=ON` 启用详细日志（包含文件名和行号）。

## 常见任务

### 修改 UI
UI 文件位于 `QtScrcpy/ui/`，包含 `.ui` 文件（Qt Designer）和对应的 `.cpp/.h` 文件。

### 添加新的键盘映射
1. 在 `keymap/` 目录创建新的 JSON 文件
2. 按照 `docs/KeyMapDes.md` 中的格式编写映射规则
3. 在应用中点击"刷新脚本"即可识别

### 更新 scrcpy-server
1. 使用 Android Studio 打开项目根目录的 server 项目
2. 编译生成 APK
3. 重命名为 `scrcpy-server`（无扩展名）
4. 替换 `QtScrcpy/QtScrcpyCore/src/third_party/scrcpy-server`

### 调试服务器
参见 `docs/DEVELOP.md` 中的"Debug the server"部分。

## 贡献指南

1. PR 请提交到 `dev` 分支，而非 `master` 分支
2. 提交前请 rebase 原项目
3. 遵循"小量多次"原则（一个 PR 一个改动）
4. 保持代码风格与现有代码一致

## 参考资源

- 原始 scrcpy 项目：https://github.com/Genymobile/scrcpy
- Qt 文档：https://doc.qt.io/
- FFmpeg 文档：https://ffmpeg.org/documentation.html
- Android 输入事件：https://developer.android.com/reference/android/view/InputEvent
