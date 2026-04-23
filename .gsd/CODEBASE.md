# Codebase Map

Generated: 2026-04-23T04:38:49Z | Files: 213 | Described: 0/213
<!-- gsd:codebase-meta {"generatedAt":"2026-04-23T04:38:49Z","fingerprint":"e47257ab28770ffd2ce8a7081f4daaddaf0fa0c0","fileCount":213,"truncated":false} -->

### (root)/
- `.gitignore`
- `build.bat`
- `CMakeLists.txt`
- `CMakePresets.json`
- `find_fonts.py`
- `README.md`
- `single_stroke_demo.py`
- `single_stroke_fonts_compare.py`

### .github/
- `.github/copilot-instructions.md`

### .superpowers/brainstorm/367-1776127086/content/
- `.superpowers/brainstorm/367-1776127086/content/layout-options.html`
- `.superpowers/brainstorm/367-1776127086/content/page-tabs.html`
- `.superpowers/brainstorm/367-1776127086/content/tab-navigation.html`

### .superpowers/brainstorm/367-1776127086/state/
- `.superpowers/brainstorm/367-1776127086/state/events`
- `.superpowers/brainstorm/367-1776127086/state/server-stopped`
- `.superpowers/brainstorm/367-1776127086/state/server.pid`

### bigfonts/
- `bigfonts/bigfont.shx`
- `bigfonts/gbcbig.shx`
- `bigfonts/HZDX.SHX`
- `bigfonts/romans.shx`

### docs/
- `docs/2d_cad_core_roadmap.md`
- `docs/Architecture_and_Core_Implementation.md`
- `docs/jog_fix.md`
- `docs/Qt_CPP_Tutorial.md`

### docs/superpowers/plans/
- `docs/superpowers/plans/2026-04-12-fix-ui-freeze-30k-lines.md`
- `docs/superpowers/plans/2026-04-14-tab-page-navigation.md`
- `docs/superpowers/plans/2026-04-18-property-panel.md`

### docs/superpowers/specs/
- `docs/superpowers/specs/2026-04-11-gcode-visualization-design.md`
- `docs/superpowers/specs/2026-04-11-text-to-gcode-design.md`
- `docs/superpowers/specs/2026-04-14-tab-page-navigation-design.md`
- `docs/superpowers/specs/2026-04-18-property-panel-design.md`

### resources/
- `resources/resources.qrc`
- `resources/style.qss`

### resources/fonts/
- `resources/fonts/basic.json`
- `resources/fonts/cursive.jhf`
- `resources/fonts/futural.jhf`
- `resources/fonts/futuram.jhf`
- `resources/fonts/gothiceng.jhf`
- `resources/fonts/rowmand.jhf`
- `resources/fonts/rowmans.jhf`
- `resources/fonts/scriptc.jhf`
- `resources/fonts/scripts.jhf`
- `resources/fonts/timesi.jhf`
- `resources/fonts/timesr.jhf`

### src/
- `src/App.cpp`
- `src/App.h`
- `src/ApplicationCore.cpp`
- `src/ApplicationCore.h`
- `src/main.cpp`
- `src/SettingsManager.cpp`
- `src/SettingsManager.h`

### src/grbl/
- `src/grbl/DxfParser.cpp`
- `src/grbl/DxfParser.h`
- `src/grbl/FileManager.cpp`
- `src/grbl/FileManager.h`
- `src/grbl/GcodeParser.cpp`
- `src/grbl/GcodeParser.h`
- `src/grbl/GcodeStreamer.cpp`
- `src/grbl/GcodeStreamer.h`
- `src/grbl/GrblConstants.h`
- `src/grbl/GrblParser.cpp`
- `src/grbl/GrblParser.h`
- `src/grbl/ImageVectorizer.cpp`
- `src/grbl/ImageVectorizer.h`
- `src/grbl/MachineState.cpp`
- `src/grbl/MachineState.h`
- `src/grbl/SerialManager.cpp`
- `src/grbl/SerialManager.h`
- `src/grbl/types.h`

### src/grbl/GraphicsDesign/
- *(39 files: 20 .h, 19 .cpp)*

### src/grbl/GraphicsDesign/Tools/
- *(29 files: 15 .h, 14 .cpp)*

### src/grbl/TextToGcode/
- `src/grbl/TextToGcode/HersheyFontParser.cpp`
- `src/grbl/TextToGcode/HersheyFontParser.h`
- `src/grbl/TextToGcode/HpglFontParser.cpp`
- `src/grbl/TextToGcode/HpglFontParser.h`
- `src/grbl/TextToGcode/JsonFontParser.cpp`
- `src/grbl/TextToGcode/JsonFontParser.h`
- `src/grbl/TextToGcode/ShxFontParser.cpp`
- `src/grbl/TextToGcode/ShxFontParser.h`
- `src/grbl/TextToGcode/SingleStrokeConverter.cpp`
- `src/grbl/TextToGcode/SingleStrokeConverter.h`
- `src/grbl/TextToGcode/SvgFontParser.cpp`
- `src/grbl/TextToGcode/SvgFontParser.h`
- `src/grbl/TextToGcode/TextToGcodeGenerator.cpp`
- `src/grbl/TextToGcode/TextToGcodeGenerator.h`
- `src/grbl/TextToGcode/VectorFontLoader.cpp`
- `src/grbl/TextToGcode/VectorFontLoader.h`
- `src/grbl/TextToGcode/VectorFontParser.h`

### src/ui/
- *(27 files: 12 .cpp, 12 .h, 3 .ui)*

### src/ui/canvas/
- `src/ui/canvas/CanvasDrawing.cpp`
- `src/ui/canvas/CanvasDrawing.h`
- `src/ui/canvas/CanvasSnap.cpp`
- `src/ui/canvas/CanvasSnap.h`
- `src/ui/canvas/CoordinateTransform.h`
- `src/ui/canvas/EntityClipboard.cpp`
- `src/ui/canvas/EntityClipboard.h`
- `src/ui/canvas/ToolManager.cpp`
- `src/ui/canvas/ToolManager.h`

### src/ui/pages/
- `src/ui/pages/GraphicsDesignPage.cpp`
- `src/ui/pages/GraphicsDesignPage.h`
- `src/ui/pages/MainPage.cpp`
- `src/ui/pages/MainPage.h`

### src/ui/panels/
- `src/ui/panels/ConnectionPanel.cpp`
- `src/ui/panels/ConnectionPanel.h`
- `src/ui/panels/ConnectionPanel.ui`
- `src/ui/panels/ConsolePanel.cpp`
- `src/ui/panels/ConsolePanel.h`
- `src/ui/panels/ControlPanel.cpp`
- `src/ui/panels/ControlPanel.h`
- `src/ui/panels/ControlPanel.ui`
- `src/ui/panels/GraphicsDesignPanel.cpp`
- `src/ui/panels/GraphicsDesignPanel.h`
- `src/ui/panels/JogPanel.cpp`
- `src/ui/panels/JogPanel.h`
- `src/ui/panels/JogPanel.ui`
- `src/ui/panels/SettingsPanel.cpp`
- `src/ui/panels/SettingsPanel.h`
- `src/ui/panels/StatusPanel.cpp`
- `src/ui/panels/StatusPanel.h`
- `src/ui/panels/StatusPanel.ui`

### tests/
- `tests/test_gcode_parser.cpp`
- `tests/test_shx_debug.cpp`
