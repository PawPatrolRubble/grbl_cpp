# GrblSender — GRBL G-Code Sender

A Qt6/C++ desktop application for sending G-code to GRBL-powered CNC machines.

## Features

- **Serial Connection** — Port/baud selection, auto-discover available COM ports
- **G-code Streaming** — Load `.nc`/`.gcode` files, stream line-by-line with GRBL buffer management
- **Jog Control** — Continuous mode (hold to move, release to stop) and step mode
- **Feed/Spindle Overrides** — Adjust feed rate and spindle speed in real-time
- **Real-time Status** — Position, state, feed, and spindle displayed live (250ms polling)
- **Streaming Controls** — Start, pause, resume, stop G-code streaming
- **Keyboard Jogging** — Arrow keys + PgUp/PgDn for XYZ axis control
- **Quick Commands** — Home (`$H`), Unlock (`$X`), Reset, Status report (`?`)
- **Console Log** — Full log of all GRBL responses

## Architecture

```
src/
├── main.cpp              — Application entry point
├── App.h/cpp             — Application bootstrap
├── grbl/                 — Core GRBL protocol layer
│   ├── types.h            — Shared types (MachinePosition, StreamProgress)
│   ├── MachineState.h/cpp — Machine state model
│   ├── GrblParser.h/cpp   — Parses GRBL status reports <Idle|Wpos:|...>
│   ├── SerialManager.h/cpp — Win32 overlapped serial I/O + G-code streaming
│   ├── GcodeParser.h/cpp  — Parses G-code lines into motion commands
│   └── FileManager.h/cpp  — File I/O
└── ui/                   — Presentation layer
    ├── MainWindow.h
    └── MainWindow.cpp
```

## Jog Control

Two modes available via the **Mode** dropdown:

| Mode | Behavior |
|------|----------|
| **Continuous** | Press and hold button → axis moves continuously at feed rate. Release → jog cancel (`0x85`) stops motion immediately |
| **Step** | Each button click sends one fixed-distance incremental jog |

Commands sent to GRBL:
- **Continuous jog**: `$J=G91 {axis}{sign}9999 F{feed}` — large distance keeps axis moving until cancelled
- **Step jog**: `$J=G91 {axis}{sign}{step} F{feed}`
- **Jog cancel**: realtime byte `0x85`

## Build Requirements

| Component | Version |
|-----------|---------|
| Visual Studio | 2026 (MSVC 19.50+) |
| Qt | 6.11.0 MSVC 2022 64-bit at `C:\Qt\6.11.0\msvc2022_64` |
| CMake | 3.16+ |
| Platform | Windows |

## Build

### Quick build
```batch
D:\projects\github\grbl_cpp\build.bat
```

### Manual build
```batch
cmake --build D:\projects\github\grbl_cpp\build --config Release
```

### After code changes
```batch
cmake --build D:\projects\github\grbl_cpp\build --config Release
```

## Debugging (VS Code)

### Required Extensions
Install these from the VS Code Extensions panel (`Ctrl+Shift+X`):

| Extension | ID |
|-----------|-----|
| C/C++ | `ms-vscode.cpptools` |
| C/C++ Extension Pack | `ms-vscode.cpptools-extension-pack` |
| CMake Tools | `ms-vscode.cmake-tools` |

### Setup
1. Open VS Code at the project root:
   ```
   code D:\projects\github\grbl_cpp
   ```
2. Reload the window so CMake Tools detects the kit
3. Select **Release** configuration from the bottom toolbar
4. CMake will auto-configure with the correct Qt6 prefix path

### Debug
1. Open a source file (e.g. `src/grbl/SerialManager.cpp`)
2. Click in the **left gutter** to set breakpoints (red dots)
3. Press **F5** to start debugging
4. Use the **Debug Console** to see output

### Available Debug Targets
- **Launch GrblSender (Debug)** — Debug mode with symbols, watches work
- **Launch GrblSender (Console)** — Launches in external terminal

### Tasks (Ctrl+Shift+P → "Tasks: Run Task")
- **Build Release** — Builds with MSVC
- **Configure CMake** — Re-run CMake configuration
- **Clean Build** — Clean build artifacts

## Run

```
D:\projects\github\grbl_cpp\build\bin\Release\GrblSender.exe
```

## GRBL Protocol Reference

### Realtime Commands
| Byte | Action |
|------|--------|
| `?` | Status report request |
| `!` | Feed hold |
| `~` | Cycle start / resume |
| `0x85` | Jog cancel |
| `0x18` | Soft reset |

### Status Report Format
```
<Idle,MPos:0.000,0.000,0.000,WPos:0.000,0.000,0.000,Fs:0,0>
```

### Common G-code Commands
| Command | Description |
|---------|-------------|
| `$H` | Homing cycle |
| `$X` | Alarm release / unlock |
| `$J=G91 X...` | Jog (incremental) |
| `G0 X Y Z` | Rapid move |
| `G1 X Y Z F` | Linear move |
| `G10 L20 P0 X0 Y0 Z0` | Set work coordinate origin |
