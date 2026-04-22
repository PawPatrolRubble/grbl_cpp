# GRBL G-Code Sender - 软件架构与核心实现

本文档详细描述项目的软件架构、核心模块实现机制及关键技术点。

---

## 1. 项目概述

**GRBL G-Code Sender** 是一个基于 Qt 6 和 C++20 开发的跨平台 CNC 机器控制软件，用于与 GRBL 固件的 CNC 控制器进行通信。主要功能包括：

- **串口通信**：与 GRBL 设备建立连接，发送 G-code 命令
- **G-code 流式传输**：支持大文件的高效传输，带进度追踪
- **实时状态监控**：解析 GRBL 状态报告，显示机器位置、状态、速度等
- **手动控制（Jog）**：键盘/按钮控制机器移动
- **刀路可视化**：解析 G-code 文件并渲染 2D/3D 刀路
- **图形设计模块**：内置 CAD 功能，支持绘制线条、矩形、圆形、弧线、文本
- **文本转 G-code**：支持矢量字体（Hershey、SHX、SVG、HPGL、JSON）
- **图像矢量化**：将位图图像转换为矢量路径
- **DXF 导入**：支持 AutoCAD DXF 文件解析

---

## 2. 软件架构

### 2.1 分层架构

项目采用清晰的分层架构，UI 层与业务逻辑层完全分离：

```
┌─────────────────────────────────────────────────────────────────┐
│                         UI Layer                                │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │MainWindow│ │ Panels   │ │ Canvas   │ │ Dialogs  │           │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │
│       │           │            │            │                   │
│       └───────────┼────────────┼────────────┼───────────────────┤
│                   │            │            │                   │
│                   ▼            ▼            ▼                   │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                   ApplicationCore (Facade)                  ││
│  │  - 统一的 API 入口                                          ││
│  │  - 信号聚合与转发                                           ││
│  └─────────────────────────────────────────────────────────────┘│
│                              │                                  │
└──────────────────────────────┼──────────────────────────────────┘
                               │
┌──────────────────────────────┼──────────────────────────────────┐
│                         Core Layer                               │
│                              ▼                                  │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐   │
│  │SerialManager│ │GcodeStreamer│ │GrblParser │ │MachineState│   │
│  └────────────┘ └────────────┘ └────────────┘ └────────────┘   │
│                              │                                  │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐                  │
│  │GcodeParser │ │FileManager │ │SettingsMgr │                  │
│  └────────────┘ └────────────┘ └────────────┘                  │
│                              │                                  │
└──────────────────────────────┼──────────────────────────────────┘
                               │
┌──────────────────────────────┼──────────────────────────────────┐
│                    Domain Modules                               │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                  GraphicsDesign Module                       ││
│  │  Entity, Document, CommandStack, Shape, Tools               ││
│  └─────────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                  TextToGcode Module                          ││
│  │  FontParser, TextToGcodeGenerator, VectorFontLoader         ││
│  └─────────────────────────────────────────────────────────────┘│
│  ┌────────────────┐ ┌────────────────┐                         │
│  │ImageVectorizer │ │   DxfParser    │                         │
│  └────────────────┘ └────────────────┘                         │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 核心设计原则

| 原则 | 实现方式 |
|------|----------|
| **关注点分离** | UI 层不直接访问底层服务，通过 ApplicationCore 门面类交互 |
| **单向依赖** | 上层依赖下层，下层不依赖上层 |
| **信号槽解耦** | 模块间通过 Qt 信号槽通信，无直接调用 |
| **线程安全** | GcodeParser 无状态，可在任意线程调用；SerialManager 使用 QMutex |
| **责任链模式** | 响应数据流：SerialManager → GrblParser → MachineState → UI |

---

## 3. 核心模块详解

### 3.1 ApplicationCore - 服务容器与门面

**文件位置**：`src/ApplicationCore.h/cpp`

ApplicationCore 是整个后端的核心枢纽，负责：

1. **服务创建与生命周期管理**
2. **服务间连接（信号槽绑定）**
3. **为 UI 层提供统一的 API 接口**

```cpp
class ApplicationCore : public QObject {
    // 服务实例
    MachineState  *m_machineState;
    SerialManager *m_serialManager;
    GcodeStreamer *m_streamer;
    GrblParser    *m_grblParser;
    GcodeParser   *m_gcodeParser;
    FileManager   *m_fileManager;

    // 门面方法
    void startStreaming(const QStringList &lines, int startLine, int endLine);
    void pauseStreaming();
    void resumeStreaming();
    void stopStreaming();
    void sendCommand(const QString &cmd);
    void sendRealtime(char cmd);

    // 聚合信号
    void responseReceived(const QString &response);
    void connectedChanged(bool connected);
    void streamingProgress(int currentLine, int totalLines);
    ...
};
```

**服务连接流程（wireServices）**：

```cpp
void ApplicationCore::wireServices() {
    // 数据流：串口 → 解析器 → 状态
    connect(m_serialManager, &SerialManager::responseReceived,
            m_grblParser, &GrblParser::parseResponse);

    // ACK 流：串口 → 流式传输器
    connect(m_serialManager, &SerialManager::ackReceived,
            m_streamer, &GcodeStreamer::onAckReceived);

    // 命令流：流式传输器 → 串口
    connect(m_streamer, &GcodeStreamer::sendCommandRequested,
            m_serialManager, &SerialManager::sendCommand);
    ...
}
```

---

### 3.2 SerialManager - 串口通信管理

**文件位置**：`src/grbl/SerialManager.h/cpp`

核心职责：实现 GRBL 的**字符计数流式协议**。

#### 3.2.1 GRBL 字符计数协议原理

GRBL 控制器有一个 128 字节的串口接收缓冲区。发送命令时必须确保缓冲区不溢出：

```
发送命令 → 计算字节数 → 加入队列 → 检查缓冲区空间 → 写入串口
收到 "ok" → 释放对应字节数 → 触发队列继续发送
```

**关键实现**：

```cpp
constexpr int GRBL_RX_BUFFER_SIZE = 127;

// 发送队列
QQueue<QByteArray> m_txQueue;       // 待发送命令
QQueue<int> m_sentLineLengths;      // 已发送但未确认的命令长度
int m_rxBufUsed = 0;                // 已占用缓冲区字节数

int SerialManager::sendCommand(const QString &command) {
    QByteArray data = command.toUtf8() + "\n";
    m_txQueue.enqueue(data);
    drainQueue();  // 尝试发送
    return data.size();
}

void SerialManager::drainQueue() {
    while (!m_txQueue.isEmpty()) {
        const QByteArray &head = m_txQueue.head();
        if (m_rxBufUsed + head.size() > GRBL_RX_BUFFER_SIZE)
            break;  // 缓冲区不足，等待 ACK
        QByteArray data = m_txQueue.dequeue();
        m_rxBufUsed += data.size();
        m_sentLineLengths.enqueue(data.size());
        m_port->write(data);
    }
}

void SerialManager::processBuffer() {
    // 收到 "ok" 或 "error" 时释放缓冲区
    if (line.startsWith("ok") || line.startsWith("error")) {
        m_rxBufUsed -= m_sentLineLengths.dequeue();
    }
    drainQueue();  // 继续发送队列
}
```

#### 3.2.2 实时命令

实时命令直接发送，不经过队列：

```cpp
void SerialManager::sendRealtimeCommand(char command) {
    m_port->write(&command, 1);
    m_port->flush();
}
```

常用实时命令：
- `0x18 (Ctrl+X)` - 软复位
- `?` - 请求状态报告
- `0x85` - 进给保持
- `0x86` - 恢复进给
- `0x8A` - Jog 取消

---

### 3.3 GcodeStreamer - G-code 流式传输

**文件位置**：`src/grbl/GcodeStreamer.h/cpp`

核心职责：管理 G-code 文件的逐行发送，配合 SerialManager 的缓冲区管理。

#### 3.3.1 批量发送策略

避免一次性将数千行加入队列导致 UI 卡顿：

```cpp
static constexpr int MAX_PENDING_COMMANDS = 48;

void GcodeStreamer::sendNextLine() {
    int pending = m_serialManager->pendingQueueSize();

    while (m_linesSent < m_totalLines && pending < MAX_PENDING_COMMANDS) {
        QString line = m_streamQueue[m_linesSent++].trimmed();
        // 跳过空行和注释
        if (line.isEmpty() || line.startsWith(';') || line.startsWith('('))
            continue;
        emit sendCommandRequested(line);
        ++pending;
    }
}
```

#### 3.3.2 状态机

```
Idle → startStreaming() → Streaming
Streaming → pauseStreaming() → Paused
Paused → resumeStreaming() → Streaming
Streaming/Paused → stopStreaming() → Idle
Streaming → all ACKs received → Idle
```

#### 3.3.3 进度信号 throttling

避免每行都发送进度更新：

```cpp
int step = qMax(1, qMin(m_totalLines / 100, 50));
if (m_linesSent >= m_totalLines || m_linesSent - m_lastProgressLine >= step) {
    emit streamingProgress(m_linesSent, m_totalLines);
}
```

---

### 3.4 GrblParser - GRBL 响应解析

**文件位置**：`src/grbl/GrblParser.h/cpp`

核心职责：解析 GRBL 的各种响应格式。

#### 3.4.1 状态报告解析

GRBL 状态报告格式：`<Idle|WPos:0.000,0.000,0.000|FS:500,0|WCO:0,0,0>`

```cpp
void GrblParser::parseStatusReport(const QString &report) {
    QString content = report.mid(1, report.size() - 2);  // 去掉 <>
    QStringList fields = content.split('|');

    // 第一个字段是状态
    MachineState::State state = parseState(fields[0]);
    m_machineState->setState(state);

    for (const QString &field : fields) {
        if (field.startsWith("WPos:"))
            m_machineState->setWorkPosition(parsePosition(field.mid(5)));
        if (field.startsWith("MPos:"))
            m_machineState->setMachinePosition(parsePosition(field.mid(5)));
        if (field.startsWith("WCO:"))
            m_machineState->setWco(parseWco(field.mid(4)));
        if (field.startsWith("FS:"))
            parseFeedOverrides(field.mid(3));
    }
}
```

#### 3.4.2 G-code 参数解析

解析坐标系偏移：`[G54:0.000,0.000,0.000]`、`[G92:...]`、`[TLO:...]`

```cpp
void GrblParser::parseGCodeParameters(const QString &line) {
    QString content = line.mid(1, line.size() - 2);
    int colonPos = content.indexOf(':');
    QString key = content.left(colonPos);
    QString value = content.mid(colonPos + 1);
    m_wcsOffsets[key] = parsePosition(value);
    updateWcoFromOffsets();
}
```

---

### 3.5 GcodeParser - G-code 文件解析

**文件位置**：`src/grbl/GcodeParser.h/cpp`

核心职责：解析 G-code 文件为可视化刀路数据。

#### 3.5.1 数据结构

```cpp
struct GcodeMove {
    enum class Type { Rapid, Linear, ArcCW, ArcCCW };
    Type type;
    QVector3D start, end;
    double iOffset, jOffset;  // 圆弧中心偏移
    double radius;
    double feedRate, spindleSpeed;
    int lineNumber;
};
```

#### 3.5.2 状态机式解析

维护模态状态（G90/G91 等）：

```cpp
QList<GcodeMove> GcodeParser::parse(const QList<QString> &lines) {
    QVector3D currentPos;
    bool absolute = true;
    GcodeMove::Type currentMotionType = GcodeMove::Type::Rapid;

    for (const QString &line : cleaned) {
        // 解析字母+数值对
        QRegularExpressionMatchIterator it = s_regex.globalMatch(stripped);
        while (it.hasNext()) {
            QChar letter = match.captured(1).at(0).toUpper();
            double value = match.captured(2).toDouble();

            switch (letter.unicode()) {
            case 'G':
                if (value == 0) motionType = GcodeMove::Type::Rapid;
                if (value == 1) motionType = GcodeMove::Type::Linear;
                if (value == 90) absolute = true;
                if (value == 91) absolute = false;
            case 'X': target.setX(absolute ? value : currentPos.x() + value);
            case 'Y': target.setY(absolute ? value : currentPos.y() + value);
            case 'Z': target.setZ(absolute ? value : currentPos.z() + value);
            ...
            }
        }
        if (hasMotion) {
            moves.append(move);
            currentPos = target;
            currentMotionType = motionType;
        }
    }
}
```

#### 3.5.3 几何变换

支持缩放、平移、旋转：

```cpp
static QList<GcodeMove> transformMoves(const QList<GcodeMove> &moves,
                                       double scaleX, double scaleY,
                                       double offsetX, double offsetY,
                                       double rotateDeg);
```

---

### 3.6 MachineState - 机器状态模型

**文件位置**：`src/grbl/MachineState.h/cpp`

核心职责：存储和通知机器状态变化。

```cpp
class MachineState : public QObject {
    enum class State { Idle, Run, Hold, Home, Alarm, Jog, Door, Check, Sleep, Unknown };

    State m_state;
    QVector3D m_machinePos, m_workPos, m_wco;
    double m_feedRate, m_spindleSpeed;
    QMap<int, double> m_grblSettings;

signals:
    void stateChanged(State state);
    void machinePositionChanged(const QVector3D &pos);
    void workPositionChanged(const QVector3D &pos);
    void feedRateChanged(double rate);
    ...
};
```

**坐标计算**：

- **Machine Position (MPos)**：机器绝对坐标
- **Work Position (WPos)**：工件坐标 = MPos - WCO
- **WCO (Work Coordinate Offset)**：坐标系偏移 = G54 + G92 + TLO

---

### 3.7 FileManager - 文件管理

**文件位置**：`src/grbl/FileManager.h/cpp`

简单的文件加载/保存管理器，存储文件内容和路径。

---

### 3.8 GraphicsDesign Module - 图形设计系统

**文件位置**：`src/grbl/GraphicsDesign/`

#### 3.8.1 核心类

| 类 | 职责 |
|-----|------|
| **Entity** | 抽象基类，所有图形元素的基类 |
| **Document** | 文档管理，包含实体集合、选择、撤销/重做 |
| **CommandStack** | 命令栈，实现 Undo/Redo |
| **Shape** | 形状抽象，用于几何计算 |
| **Tools/** | 各种绘图工具（LineTool, RectangleTool 等） |

#### 3.8.2 Entity 基类设计

```cpp
class Entity {
    enum class Kind { Line, Rectangle, Circle, Arc, Polygon, Text };

    // 核心接口
    virtual QRectF boundingRect() const = 0;
    virtual void draw(QPainter& painter) const = 0;
    virtual double distanceTo(const QPointF& point) const = 0;  // 碰撞检测
    virtual void translate(const QPointF& delta) = 0;           // 移动
    virtual std::unique_ptr<Entity> clone() const = 0;          // 克隆

    // 编辑手柄
    virtual std::vector<EditHandle> editHandles() const = 0;
    virtual std::unique_ptr<Entity> withDraggedHandle(...) = 0;

    // G-code 输出
    virtual QList<GcodeMove> toGcodeMoves(double zSafe, double zCut, double feedRate) = 0;
};
```

#### 3.8.3 Undo/Redo 实现

采用命令模式：

```cpp
class Command {
    virtual void execute(Document& document) = 0;
    virtual void undo(Document& document) = 0;
};

class AddEntityCommand : public Command {
    void execute(Document& doc) { doc.addEntity(m_entity); }
    void undo(Document& doc) { doc.takeLastEntity(); }
};

class CommandStack {
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;

    void execute(std::unique_ptr<Command> cmd) {
        cmd->execute(m_document);
        m_undoStack.push_back(std::move(cmd));
        m_redoStack.clear();
    }

    void undo() {
        auto cmd = std::move(m_undoStack.back());
        m_undoStack.pop_back();
        cmd->undo(m_document);
        m_redoStack.push_back(std::move(cmd));
    }
};
```

---

### 3.9 TextToGcode Module - 文本转 G-code

**文件位置**：`src/grbl/TextToGcode/`

#### 3.9.1 矢量字体支持

| 解析器 | 字体格式 |
|--------|---------|
| HersheyFontParser | Hershey 矢量字体 |
| ShxFontParser | AutoCAD SHX 字体 |
| SvgFontParser | SVG 矢量字体 |
| HpglFontParser | HPGL 格式 |
| JsonFontParser | 自定义 JSON 格式 |

#### 3.9.2 字体数据结构

```cpp
// 每个字符映射到一组笔画路径
QMap<QChar, QList<QList<QPointF>>> fontData;
// 每个笔画是一系列点
QList<QPointF> stroke;
```

#### 3.9.3 生成流程

```cpp
QStringList TextToGcodeGenerator::generate(const QString& text,
                                           const QMap<QChar, ...>& fontData,
                                           const Options& options) {
    // 1. 根据起点、锚点计算每个字符位置
    // 2. 遍历每个字符的笔画
    // 3. 转换为 G0/G1 命令（抬笔移动 → 落笔绘制）
    // 4. 添加 Z 轴控制（safeZ/cutZ）
}
```

---

### 3.10 ImageVectorizer - 图像矢量化

**文件位置**：`src/grbl/ImageVectorizer.h/cpp`

将位图转换为矢量路径，用于激光雕刻/笔式绘图。

#### 3.10.1 处理管道

```
Load Image → Grayscale → Threshold/Edge → Thin → Extract Contours → Simplify → Scale
```

#### 3.10.2 关键算法

**Zhang-Suen 细化算法**：将二值图像细化为单像素宽线条

**Ramer-Douglas-Peucker 简化**：减少路径点数

```cpp
QList<QPointF> rdpSimplify(const QList<QPointF>& points, double epsilon) {
    // 递归分割，移除距离小于 epsilon 的点
}
```

---

### 3.11 DxfParser - DXF 文件解析

**文件位置**：`src/grbl/DxfParser.h/cpp`

解析 AutoCAD DXF 格式，提取几何实体。

支持的实体类型：
- LINE
- CIRCLE
- ARC
- LWPOLYLINE
- POLYLINE
- SPLINE
- ELLIPSE

---

## 4. UI 层架构

### 4.1 MainWindow 结构

MainWindow 采用 DockWidget 架构，各面板可自由排列：

```cpp
class MainWindow : public QMainWindow {
    ApplicationCore *m_core;  // 后端门面

    // Dock 面板
    DockPanel *m_dockConnection;
    DockPanel *m_dockStatus;
    DockPanel *m_dockControl;
    DockPanel *m_dockJog;
    DockPanel *m_dockToolpath;
    DockPanel *m_dockConsole;
    DockPanel *m_dockDesign;
    DockPanel *m_dockSettings;

    // 内部 Widget
    ConnectionPanel *m_connectionPanel;
    StatusPanel     *m_statusPanel;
    JogPanel        *m_jogPanel;
    ToolpathPanel   *m_toolpathPanel;
    ...
};
```

### 4.2 面板职责

| 面板 | 功能 |
|------|------|
| ConnectionPanel | 串口连接配置 |
| StatusPanel | 机器状态显示 |
| ControlPanel | GRBL 控制命令（归零、复位等） |
| JogPanel | 手动移动控制 |
| ConsolePanel | 命令输入/输出日志 |
| ToolpathPanel | 刀路可视化 |
| GraphicsDesignPanel | 图形设计界面 |
| SettingsPanel | 应用设置 |

### 4.3 信号槽连接模式

面板不直接调用后端服务，而是通过信号通知 MainWindow：

```cpp
// ConnectionPanel 发射信号
emit connectRequested(portName, baudRate);
emit disconnectRequested();

// MainWindow 连接并转发
connect(m_connectionPanel, &ConnectionPanel::connectRequested,
        this, [this](const QString &port, int baud) {
            m_core->serialManager()->connectToGrbl(port, baud);
        });
```

---

## 5. 关键实现机制

### 5.1 异步文件解析

使用 QtConcurrent 在后台线程解析大文件：

```cpp
QFutureWatcher<QList<GcodeMove>> *m_parseWatcher;

void MainWindow::loadAndParseFile(const QString &filePath) {
    QStringList lines = m_core->fileManager()->lines();

    QFuture<QList<GcodeMove>> future = QtConcurrent::run([lines]() {
        GcodeParser parser;
        return parser.parse(lines);  // 线程安全
    });

    m_parseWatcher->setFuture(future);
}

connect(m_parseWatcher, &QFutureWatcher<...>::finished, this, [this]() {
    m_toolpathPanel->setMoves(m_parseWatcher->result());
});
```

### 5.2 状态轮询机制

SerialManager 使用 QTimer 定期请求状态：

```cpp
m_pollTimer->setInterval(250);
connect(m_pollTimer, &QTimer::timeout, this, [this]() {
    sendRealtimeCommand('?');  // 状态报告请求
});
```

### 5.3 暗色主题实现

```cpp
void setupDarkPalette() {
    QColor windowColor(30, 30, 46);       // #1e1e2e
    QColor highlightColor(124, 111, 247); // #7c6ff7

    darkPalette.setColor(QPalette::Window, windowColor);
    darkPalette.setColor(QPalette::Highlight, highlightColor);
    QApplication::setPalette(darkPalette);
}

void loadStyleSheet(QApplication &app) {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QFile styleFile(":/resources/style.qss");
    app.setStyleSheet(styleFile.readAll());
}
```

---

## 6. 数据流向

### 6.1 命令发送流

```
UI Panel → emit signal → MainWindow → ApplicationCore::sendCommand()
    → SerialManager::sendCommand() → drainQueue() → QSerialPort::write()
    → GRBL 控制器
```

### 6.2 响应处理流

```
GRBL 控制器 → QSerialPort::readyRead → SerialManager::handleReadyRead()
    → processBuffer() → emit responseReceived(line)
    → GrblParser::parseResponse() → MachineState 更新
    → emit stateChanged() → UI 更新
```

### 6.3 文件执行流

```
MainWindow::startStreaming → ApplicationCore::startStreaming()
    → GcodeStreamer::startStreaming() → sendNextLine()
    → emit sendCommandRequested → SerialManager::sendCommand()
    → ... → SerialManager::ackReceived → GcodeStreamer::onAckReceived()
    → sendNextLine() → ... → streamingFinished
```

---

## 7. 配置与构建

### 7.1 CMake 配置

```cmake
cmake_minimum_required(VERSION 3.16)
project(GrblSender VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)   # 自动 MOC
set(CMAKE_AUTOUIC ON)   # 自动 UI 编译
set(CMAKE_AUTORCC ON)   # 自动资源编译

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets OpenGL OpenGLWidgets Concurrent SerialPort)

find_package(OpenCV REQUIRED)  # 图像矢量化

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets ...)
```

### 7.2 目录结构

```
src/
├── main.cpp              — 入口
├── App.h/cpp             — 应用启动
├── ApplicationCore.h/cpp — 服务容器
├── SettingsManager.h/cpp — 设置管理
├── ui/                   — UI 层
│   ├── MainWindow.h/cpp
│   ├── panels/           — 各功能面板
│   ├── ToolpathCanvas.h/cpp
│   └── DesignCanvas.h/cpp
└── grbl/                 — 业务逻辑
    ├── SerialManager.h/cpp
    ├── GcodeStreamer.h/cpp
    ├── GrblParser.h/cpp
    ├── GcodeParser.h/cpp
    ├── MachineState.h/cpp
    ├── GraphicsDesign/   — 图形设计模块
    ├── TextToGcode/      — 文字转 G-code
    └── ImageVectorizer.h/cpp
```

---

## 8. 总结

本项目的核心设计亮点：

| 特性 | 实现方式 |
|------|----------|
| **架构分离** | ApplicationCore 门面 + 分层依赖 |
| **流式传输** | GRBL 字符计数协议 + 批量发送 throttling |
| **响应解析** | 状态机式解析 + 信号驱动状态更新 |
| **线程安全** | 无状态解析器 + QMutex 保护共享数据 |
| **Undo/Redo** | 命令模式 + CommandStack |
| **矢量字体** | 多格式解析器 + 笔画路径数据结构 |
| **图像矢量化** | 细化算法 + 轮廓提取 + RDP 简化 |
| **主题系统** | Fusion Style + QPalette + QSS |