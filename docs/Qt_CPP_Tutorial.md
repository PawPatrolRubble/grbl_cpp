# Qt 与 C++ 实战教程 - 基于 GRBL G-Code Sender 项目

本教程将结合本项目的实际代码，系统讲解 Qt 与 C++ 的核心概念和实战技巧。

---

## 目录

1. [Qt 基础概念](#1-qt-基础概念)
2. [项目架构与 CMake 构建](#2-项目架构与-cmake-构建)
3. [.ui 文件与代码集成](#3-ui-文件与代码集成)
4. [信号与槽机制](#4-信号与槽机制)
5. [常用 Qt 控件](#5-常用-qt-控件)
6. [布局管理](#6-布局管理)
7. [样式与主题](#7-样式与主题)
8. [多线程与异步操作](#8-多线程与异步操作)
9. [事件处理](#9-事件处理)
10. [实战案例解析](#10-实战案例解析)

---

## 1. Qt 基础概念

### 1.1 什么是 Qt？

Qt 是一个跨平台的 C++ 应用程序开发框架，提供了丰富的 GUI 组件、网络通信、多线程等模块。本项目使用 **Qt 6.11.0** 开发一个 GRBL CNC 机器控制软件。

### 1.2 Qt 应用程序生命周期

```cpp
// src/main.cpp
#include "App.h"

int main(int argc, char *argv[])
{
    return runApplication(argc, argv);
}
```

```cpp
// src/App.cpp
int runApplication(int argc, char *argv[])
{
    QApplication app(argc, argv);  // 创建 Qt 应用实例
    setupAppInfo();                // 设置应用信息
    loadStyleSheet(app);           // 加载样式表

    MainWindow window;             // 创建主窗口
    window.show();                 // 显示窗口

    return app.exec();             // 进入事件循环
}
```

**关键点**：
- `QApplication` 是所有 Qt GUI 应用程序的入口
- `app.exec()` 启动事件循环，处理用户交互、定时器、网络事件等
- 窗口通过 `show()` 显示

---

## 2. 项目架构与 CMake 构建

### 2.1 项目结构

```
src/
├── main.cpp              — 入口点
├── App.h/cpp             — 应用启动配置
├── ui/                   — UI 层
│   ├── MainWindow.h/cpp  — 主窗口
│   ├── panels/           — 功能面板
│   │   ├── ConnectionPanel.h/cpp/.ui
│   │   ├── StatusPanel.h/cpp/.ui
│   │   ├── JogPanel.h/cpp/.ui
│   │   └── ConsolePanel.h/cpp
│   └── MainWindow.ui     — Qt Designer 设计的 UI
└── grbl/                 — 业务逻辑层
    ├── SerialManager.h/cpp
    ├── MachineState.h/cpp
    └── GcodeParser.h/cpp
```

### 2.2 CMake 基础概念

CMake 是一个跨平台的构建系统生成器，它不直接编译代码，而是生成平台特定的构建文件（如 Makefile、Visual Studio 项目、Ninja 构建文件等）。

**核心工作流程**：

```
CMakeLists.txt → CMake配置 → 构建系统 → 编译器 → 可执行文件
     (配置)        (生成)      (执行)      (编译)
```

### 2.3 CMakeLists.txt 详解

#### 2.3.1 项目基础配置

```cmake
# CMakeLists.txt (第1-13行)

# 指定最低 CMake 版本
cmake_minimum_required(VERSION 3.16)

# 定义项目名称、版本和语言
project(GrblSender VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)         # 使用 C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 强制要求该标准
set(CMAKE_CXX_EXTENSIONS OFF)       # 禁用编译器扩展（如 gnu++20）

# Qt 自动工具
set(CMAKE_AUTOMOC ON)   # 自动处理 Q_OBJECT 宏的 MOC
set(CMAKE_AUTOUIC ON)   # 自动编译 .ui 文件 → ui_*.h
set(CMAKE_AUTORCC ON)   # 自动编译 .qrc 资源文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # 生成 compile_commands.json（用于 LSP）

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
```

**CMake 变量说明**：

| 变量 | 说明 | 示例值 |
|------|------|--------|
| `CMAKE_SOURCE_DIR` | 源代码根目录 | `/path/to/project` |
| `CMAKE_BINARY_DIR` | 构建输出目录 | `/path/to/build` |
| `PROJECT_NAME` | 项目名称 | `GrblSender` |
| `PROJECT_VERSION` | 项目版本 | `1.0.0` |

#### 2.3.2 查找依赖库

```cmake
# (第15-27行)

# 平台检测和路径设置
if(WIN32)
    # Windows 平台添加 Qt 和 OpenCV 搜索路径
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/opencv/build/x64/vc16/lib")
endif()

# 查找 Qt6 模块
find_package(Qt6 REQUIRED COMPONENTS 
    Core            # 核心模块（QObject, QString, QTimer等）
    Gui             # GUI基础（QColor, QFont, QImage等）
    Widgets         # 窗口控件（QPushButton, QLabel等）
    OpenGL          # OpenGL 支持
    OpenGLWidgets   # OpenGL Widget 集成
    Concurrent      # 多线程（QtConcurrent）
    SerialPort      # 串口通信（QSerialPort）
    Core5Compat     # Qt5兼容模块
)

# 查找 Qt 私有模块（某些高级功能需要）
find_package(Qt6CorePrivate REQUIRED)

# 查找 OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

# Qt 项目标准设置
qt_standard_project_setup()
```

**find_package 工作原理**：

1. CMake 在 `CMAKE_PREFIX_PATH` 指定的路径中搜索 `<Package>Config.cmake` 文件
2. 找到后加载配置，设置 `<Package>_LIBRARIES`、`<Package>_INCLUDE_DIRS` 等变量
3. `REQUIRED` 表示如果找不到则报错终止

#### 2.3.3 源文件组织

```cmake
# (第30-196行)

# 使用变量分组管理源文件
set(APP_SOURCES
    src/ApplicationCore.cpp
    src/SettingsManager.cpp
)

set(APP_HEADERS
    src/ApplicationCore.h
    src/SettingsManager.h
)

set(GRBL_SOURCES
    src/grbl/MachineState.cpp
    src/grbl/GrblParser.cpp
    src/grbl/SerialManager.cpp
    # ... 更多源文件
)

set(GRBL_HEADERS
    src/grbl/MachineState.h
    src/grbl/GrblParser.h
    # ... 更多头文件
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/panels/ConnectionPanel.cpp
    # ... 更多 UI 源文件
)

# UI 文件（.ui）需要单独列出
set(UI_FILES
    src/ui/MainWindow.ui
    src/ui/panels/StatusPanel.ui
    src/ui/panels/JogPanel.ui
    src/ui/panels/ConnectionPanel.ui
)

# 资源文件
set(RESOURCES
    resources/resources.qrc
)

# 合并所有源文件
set(SOURCES
    src/main.cpp
    src/App.cpp
    ${APP_SOURCES}
    ${GRBL_SOURCES}
    ${UI_SOURCES}
    ${UI_FILES}    # .ui 文件也加入 SOURCES
)

set(HEADERS
    src/App.h
    ${APP_HEADERS}
    ${GRBL_HEADERS}
    ${UI_HEADERS}
)
```

**文件分组的好处**：
- 代码模块化，便于维护
- 可以针对不同模块设置不同的编译选项
- 便于后续拆分为多个库或子项目

#### 2.3.4 创建可执行目标

```cmake
# (第198-213行)

# WIN32 表示创建 Windows GUI 应用（无控制台窗口）
add_executable(${PROJECT_NAME} WIN32 
    ${SOURCES} 
    ${HEADERS} 
    ${RESOURCES}
)

# 设置头文件搜索路径
target_include_directories(${PROJECT_NAME} PRIVATE 
    src                # 项目源码目录
    ${OpenCV_INCLUDE_DIRS}  # OpenCV 头文件路径
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::CorePrivate
    Qt6::Core5Compat
    Qt6::Concurrent
    Qt6::SerialPort
    ${OpenCV_LIBS}
)
```

**目标属性说明**：

| 属性 | 说明 |
|------|------|
| `PRIVATE` | 仅当前目标使用，不传递给依赖者 |
| `PUBLIC` | 当前目标和依赖者都使用 |
| `INTERFACE` | 仅传递给依赖者使用 |

#### 2.3.5 平台特定配置

```cmake
# (第215-226行)

if(WIN32)
    # Windows Unicode 支持
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    
    # 编译选项：生成调试信息
    target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
    
    # 链接选项：生成 PDB 文件
    target_link_options(${PROJECT_NAME} PRIVATE /DEBUG)
    
    # PDB 文件输出路径
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
        PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    )
endif()
```

#### 2.3.6 构建后命令

```cmake
# (第228-243行)

# Windows: 自动部署 Qt DLL
if(WIN32 AND EXISTS "${Qt6_DIR}/../../../bin/windeployqt.exe")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-compiler-runtime
            --dir "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Deploying Qt dependencies..."
    )
endif()

# 复制资源文件到输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/bigfonts"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/bigfonts"
    COMMENT "Copying bigfonts directory..."
)
```

**生成器表达式说明**：

| 表达式 | 说明 |
|--------|------|
| `$<TARGET_FILE_DIR:target>` | 目标可执行文件的输出目录 |
| `$<TARGET_FILE:target>` | 目标可执行文件的完整路径 |

#### 2.3.7 测试目标

```cmake
# (第245-255行)

# 创建测试可执行文件
add_executable(TestGcodeParser 
    tests/test_gcode_parser.cpp 
    src/grbl/GcodeParser.cpp 
    src/grbl/TextToGcode/ShxFontParser.cpp
)

target_include_directories(TestGcodeParser PRIVATE src)
target_link_libraries(TestGcodeParser PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Concurrent 
    Qt6::Core5Compat
)

if(WIN32)
    target_compile_definitions(TestGcodeParser PRIVATE UNICODE _UNICODE)
endif()
```

### 2.4 CMake 常用命令速查

#### 变量操作

```cmake
# 设置变量
set(MY_VAR "value")
set(MY_LIST item1 item2 item3)

# 追加到列表
list(APPEND MY_LIST item4 item5)

# 获取变量值
message(STATUS "MY_VAR = ${MY_VAR}")
```

#### 条件判断

```cmake
if(WIN32)
    # Windows 平台
elseif(UNIX)
    # Linux/macOS 平台
elseif(APPLE)
    # macOS 特定
endif()

# 布尔判断
if(DEFINED MY_VAR)        # 变量已定义
if(MY_VAR)                # 变量为真
if(NOT MY_VAR)            # 变量为假
if(MY_VAR STREQUAL "value")  # 字符串相等
```

#### 循环

```cmake
# foreach 循环
foreach(item IN LISTS MY_LIST)
    message(STATUS "Item: ${item}")
endforeach()

# while 循环
while(condition)
    # ...
endwhile()
```

#### 函数和宏

```cmake
# 函数（有作用域）
function(my_function arg1 arg2)
    message(STATUS "arg1=${arg1}, arg2=${arg2}")
    set(RESULT "${arg1}_${arg2}" PARENT_SCOPE)  # 返回值
endfunction()

# 宏（无作用域，类似展开）
macro(my_macro arg1)
    message(STATUS "arg1=${arg1}")
endmacro()
```

### 2.5 CMake 构建流程

#### 配置阶段

```bash
# 生成构建系统
cmake -B build -G "Visual Studio 17 2022" -A x64 \
    -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/msvc2022_64

# 或使用 Ninja（更快）
cmake -B build -G Ninja \
    -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/msvc2022_64 \
    -DCMAKE_BUILD_TYPE=Release
```

**常用选项**：

| 选项 | 说明 |
|------|------|
| `-B <dir>` | 指定构建目录 |
| `-G <generator>` | 指定生成器 |
| `-D<VAR>=<value>` | 设置 CMake 变量 |
| `-A <arch>` | 指定架构（x64/Win32） |

#### 构建阶段

```bash
# 编译项目
cmake --build build --config Release

# 只构建特定目标
cmake --build build --target GrblSender

# 清理构建
cmake --build build --target clean
```

#### 安装阶段

```bash
# 安装到指定目录
cmake --install build --prefix /path/to/install
```

### 2.6 Qt CMake 特性

#### CMAKE_AUTOMOC

自动处理包含 `Q_OBJECT` 宏的类：

```cpp
// MyClass.h
class MyClass : public QObject
{
    Q_OBJECT  // MOC 会处理这个宏
signals:
    void mySignal();
};
```

CMake 会自动生成 `moc_MyClass.cpp` 并编译。

#### CMAKE_AUTOUIC

自动将 `.ui` 文件转换为 `ui_<name>.h`：

```
ConnectionPanel.ui → ui_ConnectionPanel.h
```

代码中直接 `#include "ui_ConnectionPanel.h"`。

#### CMAKE_AUTORCC

自动编译 `.qrc` 资源文件：

```
resources.qrc → qrc_resources.cpp
```

#### qt_standard_project_setup()

Qt 6.3+ 的标准项目设置，自动配置：
- AUTOMOC/AUTOUIC/AUTORCC
- Qt 相关的编译定义
- C++ 标准设置

### 2.7 实战：添加新面板到项目

当需要添加新的 UI 面板（如 `NewPanel`），需要修改以下位置：

```cmake
# 1. 添加 .ui 文件到 UI_FILES
set(UI_FILES
    src/ui/MainWindow.ui
    src/ui/panels/NewPanel.ui    # ← 新增
    ...
)

# 2. 添加 .cpp 文件到 UI_SOURCES
set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/panels/NewPanel.cpp   # ← 新增
    ...
)

# 3. 添加 .h 文件到 UI_HEADERS
set(UI_HEADERS
    src/ui/MainWindow.h
    src/ui/panels/NewPanel.h     # ← 新增
    ...
)
```

**记住**：.ui 文件必须加入 `SOURCES`（通过 `UI_FILES`），否则 CMake 不会处理它！

### 2.8 CMake 最佳实践

| 实践 | 说明 |
|------|------|
| 使用现代 CMake | 避免全局命令（如 `include_directories`），使用目标特定命令 |
| 模块化源文件 | 用变量分组，便于维护 |
| 遵循 PRIVATE/PUBLIC | 明确可见性，避免依赖传递问题 |
| 使用生成器表达式 | 处理配置特定逻辑 |
| 配置检查 | 使用 `check_include_file` 等检查可用性 |
| 保持 CMakeLists.txt 简洁 | 复杂逻辑移到 .cmake 模块文件 |

---

## 3. .ui 文件与代码集成

### 3.1 什么是 .ui 文件？

`.ui` 文件是 Qt Designer 创建的 XML 格式界面描述文件。CMake 会自动将其转换为 C++ 头文件（如 `ui_JogPanel.h`）。

### 3.2 .ui 文件结构示例

```xml
<!-- src/ui/panels/ConnectionPanel.ui -->
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>ConnectionPanel</class>
 <widget class="QWidget" name="connectionPanel">
  <layout class="QFormLayout" name="formLayout">
   <item row="0" column="0">
    <widget class="QLabel" name="portLabel">
     <property name="text"><string>Port:</string></property>
    </widget>
   </item>
   <item row="0" column="1">
    <widget class="QComboBox" name="portCombo">
     <property name="editable"><bool>true</bool></property>
    </widget>
   </item>
   ...
  </layout>
 </widget>
</ui>
```

### 3.3 代码中使用 .ui 文件

**头文件模式**：

```cpp
// src/ui/panels/ConnectionPanel.h
#pragma once

#include <QWidget>

namespace Ui { class ConnectionPanel; }  // 前向声明

class ConnectionPanel : public QWidget
{
    Q_OBJECT  // 必须声明，启用信号槽机制

public:
    explicit ConnectionPanel(QWidget *parent = nullptr);
    ~ConnectionPanel();

private:
    Ui::ConnectionPanel *ui;  // UI 指针
};
```

**实现文件模式**：

```cpp
// src/ui/panels/ConnectionPanel.cpp
#include "ConnectionPanel.h"
#include "ui_ConnectionPanel.h"  // 自动生成的头文件

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent), ui(new Ui::ConnectionPanel)
{
    ui->setupUi(this);  // 初始化界面
    
    // 访问 UI 控件
    ui->portCombo->addItem("COM1");
    ui->connectBtn->setEnabled(true);
}

ConnectionPanel::~ConnectionPanel()
{
    delete ui;
}
```

### 3.4 setupUi 的工作原理

`setupUi(this)` 会：
1. 根据 .ui 文件创建所有控件
2. 设置控件属性（文本、大小、样式等）
3. 创建布局结构
4. 将所有控件作为子对象添加到当前 QWidget

---

## 4. 信号与槽机制

### 4.1 核心概念

信号与槽是 Qt 的核心通信机制，实现了对象间的松耦合通信。

- **信号 (Signal)**：事件发生时发射
- **槽 (Slot)**：响应信号的函数

### 4.2 基本语法

```cpp
// 声明信号
signals:
    void connectRequested(const QString &port, int baudRate);

// 声明槽
public slots:
    void setConnected(bool connected);

// 连接信号与槽
connect(sender, &SenderClass::signalName,
        receiver, &ReceiverClass::slotName);
```

### 4.3 项目实战示例

**ConnectionPanel 发射信号**：

```cpp
// src/ui/panels/ConnectionPanel.h
signals:
    void connectRequested(const QString &portName, int baudRate);
    void disconnectRequested();
```

```cpp
// src/ui/panels/ConnectionPanel.cpp
connect(ui->connectBtn, &QPushButton::clicked, this, [this]() {
    emit connectRequested(
        extractPortName(ui->portCombo->currentText()),
        ui->baudCombo->currentText().toInt()
    );
});
```

**MainWindow 接收并处理信号**：

```cpp
// src/ui/MainWindow.cpp
connect(m_connectionPanel, &ConnectionPanel::connectRequested,
        this, [this](const QString &port, int baud) {
            m_core->serialManager()->connectToGrbl(port, baud);
        });

connect(m_connectionPanel, &ConnectionPanel::disconnectRequested,
        serial, &SerialManager::disconnectFromGrbl);
```

### 4.4 Lambda 表达式作为槽

现代 Qt (5+) 支持使用 Lambda 作为槽函数：

```cpp
// 直接在 connect 中编写处理逻辑
connect(ui->jogStepSlider, &QSlider::valueChanged, this, [this](int val) {
    double step = val / 10.0;
    ui->jogStepLabel->setText(QString("%1 mm").arg(step, 0, 'f', 1));
});
```

### 4.5 信号槽连接方式对比

| 连接方式 | 说明 |
|----------|------|
| `connect(obj, &Class::signal, obj, &Class::slot)` | 函数指针语法（推荐） |
| `connect(obj, SIGNAL(signal), obj, SLOT(slot))` | 旧式字符串语法（不推荐） |
| `connect(obj, &Class::signal, this, [lambda]...)` | Lambda 槽（灵活） |

---

## 5. 常用 Qt 控件

### 5.1 基础控件速查

| 控件 | 用途 | 本项目使用 |
|------|------|------------|
| `QLabel` | 显示文本/图片 | 状态显示、标题 |
| `QPushButton` | 按钮 | 连接、断开、Jog 按钮 |
| `QComboBox` | 下拉选择框 | 端口选择、波特率选择 |
| `QLineEdit` | 单行文本输入 | 命令输入、坐标输入 |
| `QSpinBox/QDoubleSpinBox` | 数值输入 | Feed rate、步进距离 |
| `QSlider` | 滑块 | Step 调节 |
| `QPlainTextEdit` | 多行文本 | Console 日志显示 |
| `QGroupBox` | 分组框 | Go To Position 区域 |

### 5.2 实控示例

**JogPanel 控件配置**：

```xml
<!-- .ui 文件中定义 -->
<widget class="QComboBox" name="jogModeCombo">
 <property name="currentIndex"><number>0</number></property>
 <item><property name="text"><string>Continuous</string></property></item>
 <item><property name="text"><string>Step</string></property></item>
</widget>

<widget class="QDoubleSpinBox" name="jogFeedSpin">
 <property name="minimum"><double>1</double></property>
 <property name="maximum"><double>30000</double></property>
 <property name="value"><double>1000</double></property>
 <property name="suffix"><string> mm/min</string></property>
 <property name="decimals"><number>0</number></property>
</widget>
```

**代码中访问和设置**：

```cpp
// 获取值
double feed = ui->jogFeedSpin->value();
int mode = ui->jogModeCombo->currentIndex();

// 设置值
ui->jogFeedSpin->setValue(1500);
ui->jogStepSlider->setValue(10);
```

### 5.3 输入验证

```cpp
// ConsolePanel.cpp - 数值输入验证
QRegularExpression rx(R"(^-?\d*\.?\d*$)");
ui->absXEdit->setValidator(new QRegularExpressionValidator(rx, ui->absXEdit));
```

---

## 6. 布局管理

### 6.1 Qt 布局类

| 布局类 | 说明 | 适用场景 |
|--------|------|----------|
| `QVBoxLayout` | 垂直排列 | 窗口主布局 |
| `QHBoxLayout` | 水平排列 | 按钮行、工具栏 |
| `QGridLayout` | 网格布局 | 表格状排列 |
| `QFormLayout` | 表单布局 | 标签+输入框成对 |

### 6.2 代码布局示例（ConsolePanel）

```cpp
// src/ui/panels/ConsolePanel.cpp
auto *vLayout = new QVBoxLayout(this);
vLayout->setContentsMargins(0, 0, 0, 0);
vLayout->setSpacing(0);

auto *titleLabel = new QLabel("Console");
titleLabel->setObjectName("consoleTitle");
vLayout->addWidget(titleLabel);

auto *contentLayout = new QVBoxLayout();
contentLayout->setContentsMargins(8, 8, 8, 8);
contentLayout->setSpacing(4);

m_consoleEdit = new QPlainTextEdit();
contentLayout->addWidget(m_consoleEdit);

auto *cmdLayout = new QHBoxLayout();
cmdLayout->addWidget(m_commandEdit);
cmdLayout->addWidget(sendBtn);
contentLayout->addLayout(cmdLayout);

vLayout->addLayout(contentLayout);
```

### 6.3 .ui 文件布局示例

```xml
<!-- ConnectionPanel.ui - FormLayout -->
<layout class="QFormLayout" name="formLayout">
 <item row="0" column="0">
  <widget class="QLabel" name="portLabel">
   <property name="text"><string>Port:</string></property>
  </widget>
 </item>
 <item row="0" column="1">
  <widget class="QComboBox" name="portCombo"/>
 </item>
</layout>
```

```xml
<!-- JogPanel.ui - GridLayout for direction pad -->
<layout class="QGridLayout" name="dPadGrid">
 <item row="0" column="1">
  <widget class="QPushButton" name="btnYPlus">
   <property name="text"><string>Y+</string></property>
  </widget>
 </item>
 <item row="1" column="0">
  <widget class="QPushButton" name="btnXMinus">
   <property name="text"><string>X-</string></property>
  </widget>
 </item>
 <item row="1" column="2">
  <widget class="QPushButton" name="btnXPlus">
   <property name="text"><string>X+</string></property>
  </widget>
 </item>
 ...
</layout>
```

### 6.4 嵌套布局

```cpp
// MainWindow.cpp - DockWidget 布局
splitDockWidget(m_dockConnection, m_dockStatus, Qt::Vertical);
splitDockWidget(m_dockStatus, m_dockControl, Qt::Vertical);
tabifyDockWidget(m_dockToolpath, m_dockDesign);
```

---

## 7. 样式与主题

### 7.1 Qt 样式表 (QSS)

Qt 样式表语法类似 CSS，用于定制控件外观。

```css
/* resources/style.qss */
QWidget#jogPanel {
    background-color: #1e1e2e;
}

QPushButton#connectBtn {
    background-color: #22c55e;
    border: 2px solid #22c55e;
    color: #ffffff;
    padding: 8px 16px;
}

QPushButton#connectBtn:hover {
    background-color: #16a34a;
}

QPushButton#connectBtn:disabled {
    background-color: #9ca3af;
}
```

### 7.2 动态设置样式

```cpp
// ConnectionPanel.cpp - 连接状态样式
void ConnectionPanel::setConnected(bool connected)
{
    if (connected) {
        ui->connectBtn->setStyleSheet(
            "QPushButton#connectBtn { background-color: #9ca3af; }");
        ui->disconnectBtn->setStyleSheet(
            "QPushButton#disconnectBtn { background-color: #22c55e; }");
    } else {
        ui->connectBtn->setStyleSheet(QString());
        ui->disconnectBtn->setStyleSheet(QString());
    }
}
```

### 7.3 暗色主题配置

```cpp
// src/App.cpp
void setupDarkPalette()
{
    auto darkPalette = QPalette();
    
    QColor windowColor(30, 30, 46);
    QColor highlightColor(124, 111, 247);
    QColor textBright(224, 224, 255);
    
    darkPalette.setColor(QPalette::Window, windowColor);
    darkPalette.setColor(QPalette::WindowText, textBright);
    darkPalette.setColor(QPalette::Highlight, highlightColor);
    
    QApplication::setPalette(darkPalette);
}

void loadStyleSheet(QApplication &app)
{
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    setupDarkPalette();
    
    QFile styleFile(":/resources/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(styleFile.readAll());
    }
}
```

---

## 8. 多线程与异步操作

### 8.1 Qt Concurrent 模块

本项目使用 `QtConcurrent` 进行耗时的 G-code 解析：

```cpp
// MainWindow.cpp
#include <QtConcurrent>

QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

void MainWindow::loadAndParseFile(const QString &filePath)
{
    QStringList lines = fm->lines();
    
    // 在后台线程解析
    QFuture<QList<GcodeMove>> future = QtConcurrent::run([lines]() {
        GcodeParser parser;
        return parser.parse(lines);
    });
    
    m_parseWatcher->setFuture(future);
}

// 监听完成
connect(m_parseWatcher, &QFutureWatcher<QList<GcodeMove>>::finished,
        this, [this]() {
            m_toolpathPanel->setMoves(m_parseWatcher->result());
            statusBar()->showMessage("File loaded");
        });
```

### 8.2 QTimer 定时器

```cpp
// ConsolePanel.cpp - 批量消息刷新
QTimer m_flushTimer;
m_flushTimer.setSingleShot(true);
m_flushTimer.setInterval(200);

connect(&m_flushTimer, &QTimer::timeout, this, [this]() {
    if (!m_pendingMessages.isEmpty()) {
        m_consoleEdit->appendPlainText(m_pendingMessages.join("\n"));
        m_pendingMessages.clear();
    }
});

void ConsolePanel::queueMessage(const QString &msg)
{
    m_pendingMessages.append(msg);
    if (!m_flushPending) {
        m_flushPending = true;
        m_flushTimer.start(200);
    }
}
```

---

## 9. 事件处理

### 9.1 重写事件处理函数

```cpp
// MainWindow.h
protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
```

```cpp
// MainWindow.cpp
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 键盘 Jog 控制
    if (m_jogPanel->handleKeyPress(event))
        return;
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (m_jogPanel->handleKeyRelease(event))
        return;
    QMainWindow::keyReleaseEvent(event);
}
```

### 9.2 JogPanel 键盘处理

```cpp
// JogPanel.cpp
bool JogPanel::handleKeyPress(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return true;  // 忽略自动重复

    switch (event->key()) {
    case Qt::Key_Right:  doJog("X",  1); return true;
    case Qt::Key_Left:   doJog("X", -1); return true;
    case Qt::Key_Up:     doJog("Y",  1); return true;
    case Qt::Key_Down:   doJog("Y", -1); return true;
    case Qt::Key_PageUp: doJog("Z",  1); return true;
    case Qt::Key_PageDown: doJog("Z", -1); return true;
    default: return false;
    }
}
```

### 9.3 事件过滤器

```cpp
// MainWindow.cpp - TabBar 拖拽实现
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    auto *tabBar = qobject_cast<QTabBar *>(obj);
    if (!tabBar)
        return QMainWindow::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        tabBar->setProperty("_dragTabIndex", tabBar->tabAt(me->pos()));
    }
    // ... 处理拖拽逻辑
    
    return QMainWindow::eventFilter(obj, event);
}
```

---

## 10. 实战案例解析

### 10.1 完整面板开发流程

以 `JogPanel` 为例，演示从 .ui 文件到功能实现的完整流程：

**步骤 1：创建 .ui 文件**

```xml
<!-- src/ui/panels/JogPanel.ui -->
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>JogPanel</class>
 <widget class="QWidget" name="jogPanel">
  <layout class="QVBoxLayout" name="mainLayout">
   <!-- Mode/Step row -->
   <item>
    <layout class="QHBoxLayout" name="modeRow">
     <item><widget class="QComboBox" name="jogModeCombo"/></item>
     <item><widget class="QSlider" name="jogStepSlider"/></item>
     <item><widget class="QLabel" name="jogStepLabel"/></item>
    </layout>
   </item>
   <!-- Direction pad grid -->
   <item>
    <layout class="QGridLayout" name="dPadGrid">
     <item row="0" column="1">
      <widget class="QPushButton" name="btnYPlus"/>
     </item>
     ...
    </layout>
   </item>
  </layout>
 </widget>
</ui>
```

**步骤 2：编写头文件**

```cpp
// src/ui/panels/JogPanel.h
#pragma once

#include <QWidget>
#include <QPushButton>

class QKeyEvent;
namespace Ui { class JogPanel; }

class JogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit JogPanel(QWidget *parent = nullptr);
    ~JogPanel();

    bool handleKeyPress(QKeyEvent *event);
    bool handleKeyRelease(QKeyEvent *event);

public slots:
    void setMotionEnabled(bool enabled);

signals:
    void jogContinuousRequested(const QString &axis, int direction, double feed);
    void jogStepRequested(const QString &axis, double distance, double feed);
    void jogCancelRequested();
    void goToAbsoluteRequested(double x, double y, double z, double feed);

private:
    enum class JogMode { Continuous, Step };
    void doJog(const QString &axis, int direction);

    JogMode m_jogMode = JogMode::Continuous;
    Ui::JogPanel *ui;
};
```

**步骤 3：实现功能**

```cpp
// src/ui/panels/JogPanel.cpp
#include "JogPanel.h"
#include "ui_JogPanel.h"
#include <QRegularExpressionValidator>
#include <QKeyEvent>

JogPanel::JogPanel(QWidget *parent)
    : QWidget(parent), ui(new Ui::JogPanel)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);

    // 输入验证
    QRegularExpression rx(R"(^-?\d*\.?\d*$)");
    ui->absXEdit->setValidator(new QRegularExpressionValidator(rx, ui->absXEdit));

    // Step 滑块响应
    connect(ui->jogStepSlider, &QSlider::valueChanged, this, [this](int val) {
        double step = val / 10.0;
        ui->jogStepLabel->setText(QString("%1 mm").arg(step, 0, 'f', 1));
    });

    // Mode 选择响应
    connect(ui->jogModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        m_jogMode = static_cast<JogMode>(idx);
    });

    // Jog 按钮设置
    auto setupJogBtn = [this](QPushButton *btn, const QString &axis, int dir) {
        connect(btn, &QPushButton::pressed, this, [this, axis, dir]() {
            doJog(axis, dir);
        });
        connect(btn, &QPushButton::released, this, [this]() {
            if (m_jogMode == JogMode::Continuous)
                emit jogCancelRequested();
        });
    };

    setupJogBtn(ui->btnXPlus, "X", 1);
    setupJogBtn(ui->btnXMinus, "X", -1);
    setupJogBtn(ui->btnYPlus, "Y", 1);
    setupJogBtn(ui->btnYMinus, "Y", -1);
    setupJogBtn(ui->btnZPlus, "Z", 1);
    setupJogBtn(ui->btnZMinus, "Z", -1);

    // Go To 按钮
    connect(ui->goToAbsBtn, &QPushButton::clicked, this, [this]() {
        emit goToAbsoluteRequested(
            ui->absXEdit->text().toDouble(),
            ui->absYEdit->text().toDouble(),
            ui->absZEdit->text().toDouble(),
            ui->jogFeedSpin->value()
        );
    });
}

void JogPanel::doJog(const QString &axis, int direction)
{
    double feed = ui->jogFeedSpin->value();
    if (m_jogMode == JogMode::Continuous) {
        emit jogContinuousRequested(axis, direction, feed);
    } else {
        double step = ui->jogStepSlider->value() / 10.0;
        double signedStep = (direction >= 0) ? step : -step;
        emit jogStepRequested(axis, signedStep, feed);
    }
}

void JogPanel::setMotionEnabled(bool enabled)
{
    ui->btnXPlus->setEnabled(enabled);
    ui->btnXMinus->setEnabled(enabled);
    ui->btnYPlus->setEnabled(enabled);
    ui->btnYMinus->setEnabled(enabled);
    ui->btnZPlus->setEnabled(enabled);
    ui->btnZMinus->setEnabled(enabled);
    ui->goToAbsBtn->setEnabled(enabled);
}
```

**步骤 4：添加到 CMakeLists.txt**

```cmake
set(UI_FILES
    src/ui/panels/JogPanel.ui
    ...
)
```

**步骤 5：在 MainWindow 中集成**

```cpp
// MainWindow.cpp
m_jogPanel = new JogPanel();
m_dockJog = createDock("Jog Control", m_jogPanel, Qt::LeftDockWidgetArea);

connect(m_jogPanel, &JogPanel::jogContinuousRequested,
        this, [this](const QString &axis, int dir, double feed) {
    m_core->serialManager()->jogContinuous(axis, dir, feed);
});
```

---

## 总结

本教程通过 GRBL G-Code Sender 项目展示了 Qt 与 C++ 开发的核心技能：

| 技术点 | 项目实践 |
|--------|----------|
| UI 设计 | .ui 文件 + Qt Designer |
| 信号槽通信 | Panel ↔ MainWindow ↔ SerialManager |
| 布局管理 | QVBoxLayout, QHBoxLayout, QGridLayout |
| 控件使用 | QPushButton, QComboBox, QSpinBox, QSlider |
| 样式定制 | QSS + QPalette 暗色主题 |
| 多线程 | QtConcurrent 异步 G-code 解析 |
| 事件处理 | 键盘 Jog 控制、事件过滤器 |

---

## 扩展阅读

- [Qt 官方文档](https://doc.qt.io/qt-6/)
- [Qt 信号与槽机制详解](https://doc.qt.io/qt-6/signalsandslots.html)
- [Qt 样式表参考](https://doc.qt.io/qt-6/stylesheet-reference.html)
- [Qt Concurrent 模块](https://doc.qt.io/qt-6/qtconcurrent-index.html)