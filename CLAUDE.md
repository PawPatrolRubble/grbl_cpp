# GrblSender — Project Instructions

## Tech Stack

C++20 · Qt 6.11.0 (Widgets, OpenGL, SerialPort) · OpenCV · CMake 3.16+ · MSVC 2022 · Windows x64

## Code Style

- **Naming:** PascalCase for classes/files, camelCase for methods
- **Headers:** Use `.h` extension
- **Qt MOC:** Classes with signals/slots need `Q_OBJECT` macro
- **Memory:** Use `std::unique_ptr` for entity ownership; Qt parents manage widget lifetime

## Architecture Patterns

- **Entity system:** Polymorphic base class with virtual `draw()`, `clone()`, `toJson()`, `toGcodeMoves()`
- **Tool pattern:** Tools receive mouse events, modify Document via CommandStack
- **Service container:** ApplicationCore owns backend services, exposes facade signals to UI
- **Undo/redo:** CommandStack stores operations; Document uses it for all modifications

## Testing

- Location: `tests/`
- Pattern: `test_<module>.cpp` console applications
- Run: `build\bin\Debug\TestGcodeParser.exe` (after building Debug)
- No external framework — use Qt Core for console apps

## Build & Run

```batch
# Configure (first time)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/msvc2022_64

# Build
cmake --build build --config Release

# Run
build\bin\Release\GrblSender.exe
```

## Project Structure

```
src/
├── ApplicationCore.h/cpp    — Service container (owns SerialManager, GcodeStreamer, etc.)
├── grbl/
│   ├── SerialManager        — Serial I/O for GRBL
│   ├── GcodeStreamer        — Buffered G-code streaming
│   ├── GcodeParser          — Parse G-code to GcodeMove
│   ├── GrblParser           — Parse GRBL status reports
│   ├── GraphicsDesign/      — CAD editor (Document, Entity system, Tools)
│   │   ├── Document         — Entity management, selection, layers
│   │   ├── Entity           — Base class for drawable shapes
│   │   ├── CommandStack     — Undo/redo framework
│   │   └── Tools/           — Interactive tools (SelectTool, LineTool, etc.)
│   └── TextToGcode/         — Vector font parsing, text-to-G-code
└── ui/
    ├── MainWindow           — Main application window
    ├── DesignCanvas         — 2D CAD editor widget
    ├── ToolpathCanvas       — G-code toolpath visualization
    ├── panels/              — Dock panels (Connection, Status, Jog, etc.)
    └── canvas/              — Canvas helpers (CanvasDrawing, CanvasSnap, ToolManager)
```

## Key Files for Common Tasks

| Task | Files |
|------|-------|
| Add entity type | `src/grbl/GraphicsDesign/<Name>Entity.h/cpp` + register in CMakeLists.txt |
| Add tool | `src/grbl/GraphicsDesign/Tools/<Name>Tool.h/cpp` + add to ToolManager |
| Modify serial | `src/grbl/SerialManager.h/cpp` |
| Change G-code output | `Entity::toGcodeMoves()` in entity classes |
| Add snap type | `src/ui/canvas/CanvasSnap.cpp` |
| New UI panel | `src/ui/panels/<Name>Panel.h/cpp` + add to MainWindow |

## Conventions

- **Git branches:** `gsd/` prefix for workflow branches, `feature/`, `fix/`
- **Git commits:** Conventional commits (`feat(scope):`, `fix(scope):`, `refactor(scope):`)
- **Error handling:** Qt signals for async errors; return values for synchronous operations
- **Entity editing:** Always use CommandStack for modifications (enables undo)

## Dependencies

- Qt: `C:/Qt/6.11.0/msvc2022_64` (hardcoded in CMakeLists.txt)
- OpenCV: `C:/opencv/build/x64/vc16/lib` (hardcoded in CMakeLists.txt)