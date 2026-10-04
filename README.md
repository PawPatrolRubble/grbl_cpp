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
- **Pen Definitions and Path Assignment** — Save named pens with display colors, assign them to several CAD paths, and undo assignments together

## Architecture

```
src/
├── main.cpp              — Application entry point
├── App.h/cpp             — Application bootstrap
├── grbl/                 — Core GRBL protocol layer
│   ├── types.h            — Shared types (MachinePosition, StreamProgress)
│   ├── MachineState.h/cpp — Machine state model
│   ├── GrblParser.h/cpp   — Parses GRBL status reports <Idle|Wpos:|...>
│   ├── GrblTransport.h    — Identified command transport interface
│   ├── SerialManager.h/cpp — Async QSerialPort adapter and FIFO byte accounting
│   ├── GcodeStreamer.h/cpp — Job ownership, manual batches, lifecycle and synchronization
│   ├── GcodeParser.h/cpp  — Parses G-code lines into motion commands
│   └── FileManager.h/cpp  — File I/O
├── plotting/             — PlotJob, central builder and program-preserving transforms
└── ui/                   — Presentation layer
    ├── MainWindow.h
    └── MainWindow.cpp
```

The build compiles reusable `GcodeModel`, `PlotModel`, `CadCore`, `CadTools`, `PlotEngine`, `GrblBackend`, `ImportServices`, `ApplicationServices` and `GrblUi` libraries. CAD data and plotting do not depend on widgets or serial I/O. Interactive CAD tools have their own widget dependency. See [the current architecture](docs/architecture.md) for ownership, command flow and extension boundaries.

Jobs own a frozen program snapshot. Manual/system commands carry separate IDs and finish before a job starts; their acknowledgements cannot advance job progress. Errors, cancellation and disconnection have distinct outcomes. Successful completion waits for an acknowledged `G4 P0.01` checkpoint. Jogging waits for a fresh Idle report before ordinary commands resume. System/EEPROM-writing blocks must use manual mode rather than buffered jobs.

Toolpath transforms preserve setup, dwell, comments, auxiliary commands and source indices. Unsupported coordinate modes and machine/rack programs disable transforms; circular arcs scale proportionally. Imports and layer edits support undo/redo, including nested layer assignments.

## Tools and pen plotting

A **Tool** is the physical implement assigned to a path. **Pen** is the currently supported tool type; the Tools editor shows that type explicitly. Each definition has a stable ID, name, availability and preview color. For a pen, the preview color represents its ink. Pen-rack stations, docking templates and calibration are separate machine configuration, linked to definitions by tool ID. Plotting options and passive rack exchanges remain specific to pens; milling operations and cutters are future work.

Existing saved pen libraries load as Pen tools with the same IDs, colors, availability, station data and calibration. Settings now save the authoritative `tools` collection, with `kind: "pen"` and `previewColor`, and accept the old `pens`/`color` keys when their replacements are absent. Design assignments retain their existing tool IDs and need no conversion. Unsupported tool types are rejected rather than used for plotting.

## Assign tools to drawing paths

1. In the design panel, click **Tools...**, or open **Settings > Tools > Manage Tools...**.
2. Click **Add tool**, enter a name, choose its preview/ink color, and leave **Available for assignment** checked. The supported type is **Pen**. Tool definitions save automatically in application settings.
3. Select paths in the **Paths** list. Use Ctrl to select separate rows or Shift to select a range.
4. Choose a tool from **Assign tool**. Every selected, unlocked drawable path receives that tool in one undoable action. The list shows the tool name and a color swatch.
5. Choose **Default** to clear an explicit assignment. These paths follow the document's **Default tool**. **Manual/current tool** keeps the existing manual single-pen workflow.
6. Click **Save** to store the design's assignments and default tool. **Load** restores them; older designs continue to load without explicit assignments.

Selecting a group assigns its drawable children. Dimensions are excluded, and text keeps one pen for all of its strokes. Ordinary edits, copies, arrays, offsets, and trim/split replacements retain the source path's assignment. New paths use Default. Display colors identify pens in the controls; the Design canvas still uses its existing layer and selection colors.

Renaming a pen preserves its assignments. Disabling it makes it unavailable for new assignments. Removing it leaves existing paths marked **Missing** so that you can reassign them. Designs reference saved pen IDs; a design moved to an installation without those definitions also shows missing references.

Single-pen designs can generate G-code with the selected pen loaded manually. For designs using multiple tools, click **Generate** to set up **Automatic passive rack exchanges** and choose the initial holder contents. You can also set these options directly in Design. The button remains available for rack setup; validation errors are shown before a job can be generated. An unavailable assigned tool blocks generation in either mode.

The Toolpath page's G-code list shows each assigned tool's preview-color swatch and name beside its commands. Hover a row for the exact hex color, source path and operation. Swatches identify tool colors; text colors continue to identify motion types. Rack and synchronization rows are labeled **Rack** and **Wait** with the pen they handle. Setup and finish rows carry their own labels. These annotations follow path assignments through drawing transforms and refresh when tool names or colors change. Imported G-code without assignment metadata shows **Unknown tool**, and paths using Manual/current tool show **Current tool** without an assumed color. The displayed annotations do not alter executable or exported G-code.

The Toolpath preview uses those same pen colors for straight drawing strokes and arcs, including paths following the document's default pen. Rapid travel stays dashed orange. Selected and executing strokes highlight yellow; completed strokes and strokes outside the selected streaming range retain their gray status display. White and pale strokes have an outline for visibility. A legend explains these styles above the preview. Colors refresh after pen-definition edits and remain attached to the original commands through drawing transforms. Strokes without a defined pen, including imported G-code, use the existing blue straight-line and green arc colors. Automatic rack commands remain excluded from the drawing preview. Overlapping drawing strokes render in program order.

Use **Show tool** above the Toolpath page to filter the G-code list and preview together. **All tools** shows the complete view; choosing a tool shows its commands, strokes and associated travel, including its rack and wait rows in the list. **Unassigned / setup** shows commands without a tool ID. Only tools referenced by the loaded job appear, and missing definitions remain filterable. Tool choices follow saved IDs through renames and color edits; identical labels include an ID suffix. Original line numbers, selection, streaming ranges, exported G-code and the executable job are preserved. The count reports visible and total lines; execution on another tool displays **Hidden by tool filter**. Choose **All tools** to use drawing transforms. Loading or clearing a job resets the filter.

## Rack positions and pen-change macro preview

Open **Tools...** or **Settings > Tools > Manage Tools...**. The **Tools and Pen Rack** dialog has three tabs:

| Tab | Configuration |
| --- | --- |
| **Tools and pen stations** | Define the tool's name and preview/ink color. For its Pen type, enter a unique slot name, machine X/Y, return contact Z, and pickup contact Z. Check **Station configured** when these values are ready for inspection. Edit its separate return/release and pickup/grip local templates. |
| **Rack settings** | Enter machine travel minimum/maximum coordinates, clearance Z, XY travel feed, docking Z feed, settle time, and the permitted local docking radius. Check **Rack bounds and clearance configured** to enable compilation. |
| **Macro preview** | Choose the current holder contents and requested pen. **Empty holder** skips return; **Return current pen only** skips pickup; requesting the already loaded pen generates no exchange. Select the work frame and drawing feed to restore afterward. |

Coordinates are absolute machine coordinates in millimeters. Higher Z means raised. Defaults are unconfigured examples; enter the values for your machine. Rack settings and per-pen templates save automatically in application settings, independently of design files.

The preview generates separate phases for raising to clearance, travelling to the current station, lowering to return contact, applying local release steps and settling, retracting, then approaching and picking up the next pen. It restores plotting modes at the end with the holder still raised. Each local motion has explicit `G53 G0/G1`; linear moves require an explicit positive feed. Positive `G4 P0.01` synchronization checkpoints follow motion phases, following [GRBL's interface guidance](https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface#synchronization).

The outer sequence owns contact motion, travel, settling and retraction. Return/pickup templates add mechanism-specific local motion or dwell. Supported values are `{slotX}`, `{slotY}`, `{returnZ}`, `{pickupZ}`, `{clearanceZ}`, `{travelFeed}`, `{dockFeed}` and `{settleSeconds}`. For example:

```gcode
; Example local block; this does not define a physical gripping mechanism.
G53 G1 Z{pickupZ} F{dockFeed}
G4 P{settleSeconds}
```

Use `;` for comments. Templates support explicit machine-coordinate G0/G1 and G4 dwell, with up to 256 lines / 32,768 characters and 79 bytes per expanded command block. Dwell/settle durations are limited to 3000 seconds, and numeric commands must fit GRBL's floating-point range. Unknown values, duplicate slots, conflicting station coordinates, moves outside machine bounds or the local docking radius, non-positive feeds, modal changes and unsupported commands produce a pen/phase/line-specific error. Edits invalidate the previous preview immediately.

**Macro preview** remains an inspection tool and sends no machine commands. The same compiler supplies the rack phases in automatic drawing jobs. Local templates support passive motion; actuator commands and M6 are unsupported. Automatic mode requires verified straight travel at clearance between the rack and drawing area. Corridor waypoints, pickup sensors and per-pen drawing-height offsets remain future extensions.

## Automatic passive rack exchanges

1. Configure the rack and each required pen's station and local pickup/return templates. Verify the actual motions and clear travel on the machine. Mark **Passive pickup and return verified on machine** for each used station and **Bounds and clear travel at clearance verified on machine** for the rack. Both confirmations default off; editing motion settings clears the relevant calibration flag.
2. Assign defined pens to the paths, including a defined default pen for paths using Default. Click **Generate** with multiple tools to enable **Automatic passive rack exchanges** and choose the **Initial holder**: empty or the pen already loaded. Alternatively, set these options directly in Design. Cancelling setup preserves the existing options and generates no job. Choose the plotting **Work frame** (G54–G59), shared Safe Z/Work Z and feeds. Higher Z must raise the pen, and shared heights must suit every participating pen.
3. Leave **Return final pen to rack** checked to finish with an empty holder. Uncheck it to retain the last pen raised at machine rack clearance. The design panel displays the ordered pen sequence and pickup/return counts. Click **Generate** to load the complete typed job into the toolpath panel.
4. Connect and home the machine. Ensure the selected work coordinates are correct. Startup queries retrieve offsets; after an offset change, refresh with `$G` and `$#` if preflight reports missing data. The controller needs a current idle position report. The application translates drawing bounds through the selected WCS, G92 and TLO and checks them against the calibrated machine bounds.
5. Click **Start**, identify the physical holder contents, and confirm homing, rack occupancy and shared heights in the setup dialog. A pen already loaded must have an empty return slot; all other required pens must occupy their assigned slots. Holder contents must match the Initial holder used to generate the job.

Path order is preserved, including children of groups. For Black, Black, Red, Red, Black, the pen sequence is Black → Red → Black. Adjacent paths using one pen share a load. A change raises Z, returns the current pen through its station motion, retracts, picks up the next pen, retracts, restores the work frame, travels to the next drawing entry **while still at machine clearance**, and lowers there. Station motion uses explicit G53 machine coordinates; drawing uses the selected work frame.

Rack phases use sequential correlated acknowledgements and positive G4 synchronization checkpoints. Drawing cannot begin until pickup retraction has completed. Pause suspends the phase deadline and resumes without replaying completed steps. A command error, alarm, timeout, cancellation, reset or disconnect stops the job and marks the holder unknown. The application never attempts a rack return after interruption: inspect the holder and slots, home, choose the actual Initial holder and regenerate before restarting. A passive rack has no sensor to prove a mechanical pickup succeeded.

Automatic jobs retain phase, pen, path, frame and configuration metadata. They execute from beginning to end inside the application. Raw line ranges, flat G-code export, legacy Return to origin and post-generation drawing transforms are disabled for these jobs. Regenerate from Design when changing geometry or settings. The toolpath canvas shows drawing motion in the work frame with original source indices; inspect rack travel in Macro preview and the job's full command list.

Software verification uses a simulated transport; it does not establish physical rack calibration. See the [implementation proposal](docs/proposals/0001-automatic-passive-rack-exchanges.md) and [execution design decision](docs/decisions/0001-managed-passive-rack-jobs.md).

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

## Prerequisites

- **Visual Studio 2026** with C++ desktop development workload
- **Qt 6.11.0** installed at `C:\Qt\6.11.0\msvc2022_64`
- **CMake 3.22+** (bundled with Qt Tools)
- **Windows 10/11** x64

## Compile and Run

### 1. First-time setup (configure CMake)
```batch
cmake -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/msvc2022_64
```

### 2. Build the project
```batch
cmake --build build --config Release
```

### 3. Run the application
```batch
build\bin\Release\GrblSender.exe
```

### Quick build (single command)
```batch
D:\projects\github\grbl_cpp\build.bat
```

## Testing

Configure with `BUILD_TESTING=ON` (the default), build all targets, then run:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

CTest configures the Qt offscreen platform and runtime paths. The fourteen suites cover parsing, CAD interaction, pen assignments, rack preview compilation, architecture regressions, job lifecycle/transport accounting, plot/document transactions, automatic planning/preflight, rack phase execution, operator UI/application integration, G-code pen colors, colored toolpath rendering, shared tool filtering and generic tool-library migration. `TestJobController` uses a fake transport and the same FIFO accounting class as the serial adapter. Tests do not connect to a physical machine.

`TestAutomaticRack` verifies ordered pen runs, initial/final policies, defaults, bounds and calibration invalidation. `TestRackExecution` injects errors into every non-drawing phase and covers physical checkpoints, buffered drawing, interruption, pause/deadline behavior, immediate acknowledgements and cancellation from a phase notification. `TestAutomaticRackUi` checks the holder/setup dialog, design controls, calibration editor, drawing-only preview, coordinate confidence and ApplicationCore preflight. Add `--screenshots <directory>` to that executable to render the design panel, rack editor and Start dialog.

`TestGcodePenColors` checks per-line pen mapping, swatch rendering and selection, live pen edits, source-line selection, default/current/missing pens, raw-file and clear resets, transform preservation and execution/range highlighting. Add `--screenshots <directory>` to render the annotated G-code list.

`TestToolpathPenColors` checks rendered straight and arc colors, overlapping path order, rapid styling, mouse selection, execution/range states, live edits, pale-pen visibility, defaults, missing pens, transforms, raw imports and stale asynchronous previews. Add `--screenshots <directory>` to render the multi-pen preview, live color edit, white strokes and range states.

`TestToolFilter` checks synchronized list/preview filtering, original line indices, mouse and keyboard selection, streaming ranges, hidden execution and completed-stroke caches, tool edits/deletion, duplicate names, unassigned/raw commands, asynchronous completion, job resets and transform restrictions. Add `--screenshots <directory>` to render the selected-tool and setup-only views.

`TestToolLibrary` checks legacy and current definitions/settings, stable IDs, preview-color precedence, empty collection precedence, unsupported kinds, atomic rejection, station separation/calibration and shared UI terminology. Settings cases run in separate application sessions. Add `--screenshots <directory>` to render the Tools editor with its Pen type.

### Pen-assignment regression tests

Build the console tests with `cmake --build build --config Debug --target TestPenAssignment TestToolChangeMacro`. The assignment suite exercises project save/load, application restart persistence, widget selection, assignment, Undo/Redo, missing pens, preservation through edits/copies, and rack-editor preview updates. The macro suite exercises numeric expansion, exchange phases, bounds, slots, command restrictions, and legacy settings.

For a headless run in PowerShell:

```powershell
$env:PATH = 'C:\Qt\6.11.0\msvc2022_64\bin;C:\opencv\build\x64\vc16\bin;' + $env:PATH
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\Qt\6.11.0\msvc2022_64\plugins\platforms'
& '.\build\bin\Debug\TestPenAssignment.exe'
& '.\build\bin\Debug\TestToolChangeMacro.exe'
```

The assignment test uses an isolated settings directory. Add `--screenshots <output-directory>` to render the path controls, stations, rack settings and macro preview for inspection. The macro test has no application-settings or machine side effects.

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

```batch
build\bin\Release\GrblSender.exe
```

Or use the full path:
```batch
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
