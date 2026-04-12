# Text to G-code Generator — Design

## Overview

Add a "Text to G-code" feature that converts text strings into stroke-based G-code using vector font files (SVG, HPGL, JSON, Hershey) or system fonts. A menu action opens a modal dialog where users enter text, select a font source, configure options, preview the G-code output, and generate G-code that loads into the existing GcodeListWidget.

## Architecture

### Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `VectorFontParser` (abstract) | `grbl/TextToGcode/` | Base class for parsing vector font files |
| `HersheyFontParser` | `grbl/TextToGcode/` | Parses Hershey single-stroke (.jhf) font files |
| `SvgFontParser` | `grbl/TextToGcode/` | Parses SVG path data into stroke paths |
| `HpglFontParser` | `grbl/TextToGcode/` | Parses HPGL/GLF plotter files |
| `JsonFontParser` | `grbl/TextToGcode/` | Parses custom JSON stroke definitions |
| `VectorFontLoader` | `grbl/TextToGcode/` | Format detection, factory, and `FontLoadResult` error reporting |
| `TextToGcodeGenerator` | `grbl/TextToGcode/` | Converts text string to G-code using parsed font data or system fonts |
| `TextToGcodeDialog` | `ui/` | Modal UI dialog with input, font selector, preview, generate |
| `GcodeStreamer` | `grbl/` | Streaming state machine extracted from SerialManager |
| `SerialManager` | `grbl/` | Serial I/O only — emits ack/error signals for streamer |

### Data Flow — Text to G-code

```
User enters text → VectorFontLoader loads file → Parser extracts strokes
→ TextToGcodeGenerator maps text to strokes + computes per-glyph metrics → G-code lines
→ Loaded into GcodeListWidget via FileManager
```

### Data Flow — G-code Streaming

```
ControlPanel → GcodeStreamer.startStreaming()
GcodeStreamer → SerialManager.sendCommand() (via signal)
SerialManager → GRBL → ack/error response
SerialManager.ackReceived → GcodeStreamer.onAckReceived()
SerialManager.commandErrorReceived → GcodeStreamer.onErrorReceived()
```

## Key Classes

### VectorFontParser (abstract)

```cpp
class VectorFontParser {
public:
    virtual ~VectorFontParser() = default;
    virtual QMap<QChar, QList<QList<QPointF>>> parse(QIODevice& device) = 0;
    virtual QString formatName() const = 0; // "Hershey", "SVG", "HPGL", "JSON"
};
```

### FontLoadResult — Error Propagation

```cpp
struct FontLoadResult {
    bool success = false;
    QString errorMessage;
    QMap<QChar, QList<QList<QPointF>>> glyphs;
    explicit operator bool() const { return success; }
};
```

Replaces the previous `QMap` return that silently swallowed errors. Callers can now distinguish:
- File not found
- Unsupported format
- Cannot open file
- Valid but empty font (no glyphs parsed)

### TextToGcodeGenerator

```cpp
class TextToGcodeGenerator {
public:
    enum class ToolMode { PenPlotter, Laser };
    enum class AnchorPoint { BottomLeft, Center, TopLeft };

    struct Options {
        double fontSize = 10.0;          // mm (height)
        bool maintainAspectRatio = true;
        double characterWidth = 10.0;    // used only if maintainAspectRatio is false
        double letterSpacing = 1.2;     // multiplier of glyph advance width
        double lineSpacing = 1.5;       // multiplier
        AnchorPoint anchor = AnchorPoint::BottomLeft;
        double startX = 0.0;
        double startY = 0.0;

        ToolMode toolMode = ToolMode::PenPlotter;
        double safeZ = 5.0;              // mm
        double cutZ = 0.0;               // mm
        double feedRateXY = 500.0;       // mm/min
        double plungeRateZ = 200.0;      // mm/min
    };

    QStringList generate(const QString& text,
                         const QMap<QChar, QList<QList<QPointF>>>& fontData,
                         const Options& options);

    QStringList generateFromSystemFont(const QString& text,
                                         const QFont& font,
                                         const Options& options);
};
```

**Glyph metrics computation:** Each character's bounding box is computed from its stroke points. The generator:
1. Finds min/max X/Y across all strokes for each glyph
2. Computes `scale = fontSize / glyphHeight`
3. Offsets glyphs so their minimum Y aligns to the baseline
4. Advances cursor by `bboxWidth * letterSpacing * scale` (with a 30% minimum)
5. Handles spaces with a 50% fontSize advance
6. Handles newlines with `fontSize * lineSpacing` advance

### GcodeStreamer — Extracted from SerialManager

```cpp
class GcodeStreamer : public QObject {
    Q_OBJECT
public:
    void startStreaming(const QStringList &lines);
    void pauseStreaming();
    void resumeStreaming();
    void stopStreaming();
    bool isStreaming() const;
signals:
    void sendCommandRequested(const QString &command);
    void sendRealtimeRequested(char command);
    void streamingStarted();
    void streamingFinished();
    void streamingPaused();
    void streamingResumed();
    void streamingProgress(int currentLine, int totalLines);
public slots:
    void onAckReceived();
    void onErrorReceived(const QString &error);
    void onDisconnected();
};
```

Owns all streaming state: line queue, buffer byte tracking, line counts, pause/resume. Connected to `SerialManager` via signals — no direct dependency.

### TextToGcodeDialog

```cpp
class TextToGcodeDialog : public QDialog {
    Q_OBJECT
public:
    enum class FontSource {
        SystemFont = 0,
        CustomVector = 1
    };
    // ...
};
```

Uses `FontSource` enum instead of magic-number index comparisons. Located in `src/ui/` to maintain the layer split (UI code in `ui/`, business logic in `grbl/`).

## Vector Font File Formats

### Hershey Format (.jhf)
Standard single-stroke font format for CNC plotting. Parser stub exists; full implementation parses coordinate pairs and scales to target size.

### SVG Format
SVG files with `<path>` or `<glyph>` elements. Supports:
- `M`, `L`, `H`, `V` — move/line commands
- `C`, `S`, `Q` — cubic and quadratic bezier
- `Z` — close path
- Relative and absolute coordinates
- Character identification via `id` attribute or `unicode` attribute

### HPGL/GLF Format
Stub parser for HPGL plotter files (`.hpgl`, `.glf`). Handles `PU`, `PD`, `PA` commands.

### JSON Format
```json
{
  "name": "SimpleFont",
  "units": "mm",
  "glyphs": {
    "A": [[[0,10], [5,0], [10,10]]],
    "B": [[[0,0], [0,10], [5,10], [5,0]]]
  }
}
```
Each glyph maps to an array of strokes (arrays of point pairs).

## Dialog Layout

The dialog provides:
- Text input (`QPlainTextEdit`)
- Font source selector: **System Font** (QFontComboBox) or **Custom Vector File** (file browser)
- Size, tool mode, feed speed, plunge speed, safe Z, work Z spinboxes
- Generated G-code preview (`QPlainTextEdit`, read-only)
- Update Preview / Cancel / Generate & Load buttons

## G-code Output Format

```gcode
G21 G90        ; metric, absolute
G1 Z5.0 F200   ; raise pen to safe Z
; per-character strokes...
G1 X0.000 Y10.000 F500   ; move to glyph start (scaled + offset)
G1 Z0.0 F200             ; plunge
G1 X5.000 Y5.000 F500    ; draw stroke
G1 Z5.0 F200             ; raise pen
; next stroke or character...
; End of Text to G-Code
```

- `G21` — metric mode (mm)
- `G90` — absolute positioning
- `G1 Z...` — pen up/down (driven by safeZ/cutZ/plungeRateZ)
- `G1 X... Y... F...` — linear moves at feedRateXY
- Pen Plotter mode uses Z motion; Laser mode uses `M4`/`M5` instead

## File Structure

```
src/
├── grbl/
│   ├── MachineState.h/cpp
│   ├── GrblParser.h/cpp
│   ├── SerialManager.h/cpp         (serial I/O only — no streaming state)
│   ├── GcodeStreamer.h/cpp         (streaming state machine, extracted)
│   ├── GcodeParser.h/cpp
│   ├── FileManager.h/cpp
│   ├── types.h
│   └── TextToGcode/
│       ├── VectorFontParser.h       (abstract base)
│       ├── HersheyFontParser.h/cpp
│       ├── SvgFontParser.h/cpp
│       ├── HpglFontParser.h/cpp
│       ├── JsonFontParser.h/cpp
│       ├── VectorFontLoader.h/cpp   (format detection + FontLoadResult)
│       └── TextToGcodeGenerator.h/cpp
└── ui/
    ├── MainWindow.h/cpp             (wiring through GcodeStreamer)
    ├── ToolpathCanvas.h/cpp
    ├── GcodeListWidget.h/cpp
    ├── ToolpathPanel.h/cpp
    ├── TransformToolbar.h/cpp
    ├── TextToGcodeDialog.h/cpp     (UI dialog, in ui/ layer)
    ├── TextToGcodeDialog.ui
    └── panels/
        ├── ConnectionPanel.h/cpp
        ├── StatusPanel.h/cpp
        ├── ControlPanel.h/cpp
        ├── JogPanel.h/cpp
        └── ConsolePanel.h/cpp
```

## Menu Integration

"Text to G-code..." under **File** menu, shortcut `Ctrl+T`.

## Error Handling

| Error | Mechanism |
|-------|-----------|
| File not found | `FontLoadResult::errorMessage = "File not found: ..."` |
| Unsupported format | `FontLoadResult::errorMessage = "Unsupported font format: .xyz"` |
| Cannot open file | `FontLoadResult::errorMessage = "Cannot open file: ..."` |
| No glyphs in font | `FontLoadResult::errorMessage = "No glyphs found in font file: ..."` |
| Character not in font | Skipped with cursor advance; no warning in current impl |
| Empty text | Preview cleared; generate button shows "Generation Failed" |
| Generation error | `FontLoadResult::operator bool()` returns false; dialog shows error in preview |

## Architectural Decisions

### GcodeStreamer extraction
Streaming state (line queue, buffer tracking, ok/error counting, pause/resume) moved from `SerialManager` into `GcodeStreamer`. The two communicate via Qt signals:
- `GcodeStreamer` emits `sendCommandRequested` / `sendRealtimeRequested` → `MainWindow` wires to `SerialManager`
- `SerialManager` emits `ackReceived` / `commandErrorReceived` → `MainWindow` wires to `GcodeStreamer`
- On disconnect, `MainWindow` calls `GcodeStreamer::onDisconnected()` to clean up streaming state

This keeps SerialManager focused on serial I/O and makes GcodeStreamer independently testable.

### FontLoadResult error propagation
Previously `VectorFontLoader::loadFont()` returned an empty `QMap` for all failure cases. Now returns `FontLoadResult` with specific error messages, enabling the dialog to show meaningful error text.

### UI/business logic separation
`TextToGcodeDialog` moved from `grbl/TextToGcode/` to `ui/` to maintain the architectural boundary: `grbl/` contains business logic, `ui/` contains presentation. Only the core classes (`VectorFontLoader`, `TextToGcodeGenerator`, parsers) remain in `grbl/TextToGcode/`.

### FontSource enum
`TextToGcodeDialog` uses `FontSource::SystemFont` / `FontSource::CustomVector` instead of magic index numbers, making the font source toggle logic self-documenting.

### Glyph metrics in generator
Previous `generate()` used fixed `fontSize * letterSpacing` advance per character. Now computes per-glyph bounding boxes and scales points to the target `fontSize`, producing proportional spacing.