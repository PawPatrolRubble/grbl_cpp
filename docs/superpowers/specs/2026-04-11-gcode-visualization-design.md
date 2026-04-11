# Live G-code Execution Visualization Design

Date: 2026-04-11

## Overview

Add live execution progress visualization to the GRBL G-code sender, with bidirectional sync between G-code lines and a 2D toolpath canvas.

## Architecture

A new **ToolpathPanel** widget replaces the existing G-code editor area, containing:
- **Left side**: GcodeListWidget (G-code lines with line numbers)
- **Right side**: ToolpathCanvas (2D XY toolpath visualization)

Split via vertical QSplitter within the panel.

## Components

### 1. ToolpathCanvas

File: `src/ui/ToolpathCanvas.h`, `src/ui/ToolpathCanvas.cpp`

Custom QWidget using QPainter for 2D rendering.

**Responsibilities:**
- Store and render `QList<GcodeMove>` parsed from G-code
- Color-code segments by move type
- Track and highlight executing line during streaming
- Draw animated cutter icon at machine position
- Handle pan (drag) and zoom (wheel) interactions
- Emit click signal for segment selection

**Visual Style:**
| Move Type | Color | Style |
|-----------|-------|-------|
| G0 (rapid) | Light gray (#aaaaaa) | Dashed line |
| G1 (linear) | Blue (#2266aa) | Solid line |
| G2/G3 (arcs) | Green (#22aa66) | Solid line (approximated) |

**Execution States:**
| State | Executed | Current | Remaining | Cutter |
|-------|----------|---------|-----------|--------|
| Idle | — | — | Color-coded | At current pos |
| Streaming | Faded (#666666) | Bright yellow highlight | Color-coded | Animated |
| Paused | Faded | Yellow highlight | Normal | Frozen |
| Complete | All faded | — | — | At end position |

**Cutter Icon:**
- Small filled circle (radius ~8px scaled)
- Direction arrow pointing toward next move endpoint
- Red (#cc3333) for visibility

**API:**
```cpp
class ToolpathCanvas : public QWidget {
    Q_OBJECT
public:
    void setMoves(const QList<GcodeMove>& moves);
    void setExecutingLine(int lineNumber);
    void setCutterPosition(const QVector3D& pos);
    void highlightSegment(int lineNumber);

signals:
    void segmentClicked(int lineNumber);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
};
```

**Coordinate Handling:**
- Auto-scale on load to fit viewport
- Store transformation matrix for pan/zoom
- Convert machine coords to screen coords via transform

**Arc Approximation:**
- G2/G3 arcs approximated with 8-16 line segments based on arc angle
- Use start point, end point, and implicit center from I/J or radius

**Click Detection:**
- Distance-to-line-segment calculation for each segment
- Threshold: ~10px (screen space)
- Return nearest segment's lineNumber

### 2. GcodeListWidget

File: `src/ui/GcodeListWidget.h`, `src/ui/GcodeListWidget.cpp`

QListWidget showing G-code lines.

**Responsibilities:**
- Display G-code lines with line numbers in prefix column
- Highlight current executing line during streaming
- Emit click signal for line selection
- Auto-scroll to follow execution progress

**API:**
```cpp
class GcodeListWidget : public QListWidget {
    Q_OBJECT
public:
    void setLines(const QStringList& lines);
    void highlightLine(int lineNumber);
    void selectLine(int lineNumber);

signals:
    void lineClicked(int lineNumber);

private slots:
    void onItemClicked(QListWidgetItem* item);
};
```

**Visual Style:**
- Line numbers in left margin (gray, monospace)
- G-code text (black, monospace)
- Highlighted line: yellow background (#ffff99)
- Selected line: blue selection (#3366cc / white text)

### 3. ToolpathPanel

File: `src/ui/ToolpathPanel.h`, `src/ui/ToolpathPanel.cpp`

Container widget with vertical splitter.

**Responsibilities:**
- Host GcodeListWidget (left) and ToolpathCanvas (right)
- Coordinate bidirectional sync between child widgets
- Expose signals for MainWindow integration

**API:**
```cpp
class ToolpathPanel : public QWidget {
    Q_OBJECT
public:
    void setGcodeLines(const QStringList& lines);
    void setMoves(const QList<GcodeMove>& moves);
    void setExecutingLine(int lineNumber);
    void setCutterPosition(const QVector3D& pos);

signals:
    void lineSelected(int lineNumber);

private:
    GcodeListWidget* m_listWidget;
    ToolpathCanvas* m_canvas;
    QSplitter* m_splitter;
};
```

**Internal Connections:**
```cpp
connect(m_listWidget, &GcodeListWidget::lineClicked,
        m_canvas, &ToolpathCanvas::highlightSegment);
connect(m_canvas, &ToolpathCanvas::segmentClicked,
        m_listWidget, &GcodeListWidget::selectLine);
```

## MainWindow Integration

Replace `m_editorEdit` (QTextEdit) with `ToolpathPanel`.

**New member:**
```cpp
ToolpathPanel* m_toolpathPanel;
```

**Signal Connections:**
```cpp
// File load
connect(m_controlPanel, &ControlPanel::loadFileRequested, this, [this]() {
    // ... load file ...
    m_toolpathPanel->setGcodeLines(m_fileManager->lines());
    QList<GcodeMove> moves = m_gcodeParser->parse(m_fileManager->lines());
    m_toolpathPanel->setMoves(moves);
});

// Streaming progress
connect(m_serialManager, &SerialManager::streamingProgress, this,
        [this](int current, int total) {
    m_toolpathPanel->setExecutingLine(current);
});

// Machine position (cutter animation)
connect(m_machineState, &MachineState::machinePositionChanged,
        m_toolpathPanel, &ToolpathPanel::setCutterPosition);
```

## Data Flow

```
FileManager::loadFile()
    → QStringList lines

GcodeParser::parse(lines)
    → QList<GcodeMove> moves (each has lineNumber)

ToolpathPanel::setMoves(moves)
    → ToolpathCanvas stores moves for rendering

SerialManager::streamingProgress(line, total)
    → ToolpathPanel::setExecutingLine(line)
    → GcodeListWidget::highlightLine(line)
    → ToolpathCanvas::setExecutingLine(line)

MachineState::machinePositionChanged(pos)
    → ToolpathCanvas::setCutterPosition(pos)
    → Cutter icon moves in real-time

User click on G-code line:
    → GcodeListWidget::lineClicked(line)
    → ToolpathCanvas::highlightSegment(line)
    → Canvas highlights that segment

User click on toolpath segment:
    → ToolpathCanvas::segmentClicked(line)
    → GcodeListWidget::selectLine(line)
    → List scrolls to and selects that line
```

## Implementation Order

1. ToolpathCanvas - core rendering, pan/zoom, click detection
2. GcodeListWidget - list display, highlight, click signal
3. ToolpathPanel - container with splitter and internal sync
4. MainWindow - integrate panel, wire signals

## Testing Considerations

- Test with sample G-code files containing G0, G1, G2, G3 moves
- Verify arc approximation renders correctly
- Test pan/zoom with large toolpaths
- Verify sync: click line → segment highlights, click segment → line selects
- Test streaming animation with mock progress signals