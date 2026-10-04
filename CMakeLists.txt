cmake_minimum_required(VERSION 3.22)
project(GrblSender VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Qt6 and OpenCV: provide path hints for Qt Creator
if(WIN32)
    # Always append these paths to help Qt Creator find dependencies
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/opencv/build/x64/vc16/lib")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Concurrent SerialPort Core5Compat)


find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

qt_standard_project_setup()

# --- ApplicationCore (service container) ---
set(APP_SOURCES
    src/ApplicationCore.cpp
    src/SettingsManager.cpp
)

set(APP_HEADERS
    src/ApplicationCore.h
    src/SettingsManager.h
)

# --- GRBL backend ---
set(GRBL_SOURCES
    src/grbl/ToolLibrary.cpp
    src/grbl/ToolChangeMacroCompiler.cpp
    src/grbl/MachineState.cpp
    src/grbl/GrblParser.cpp
    src/grbl/SerialManager.cpp
    src/grbl/GcodeStreamer.cpp
    src/grbl/GcodeParser.cpp
    src/grbl/FileManager.cpp
    src/grbl/TextToGcode/HersheyFontParser.cpp
    src/grbl/TextToGcode/SvgFontParser.cpp
    src/grbl/TextToGcode/HpglFontParser.cpp
    src/grbl/TextToGcode/JsonFontParser.cpp
    src/grbl/TextToGcode/ShxFontParser.cpp
    src/grbl/TextToGcode/VectorFontLoader.cpp
    src/grbl/TextToGcode/TextToGcodeGenerator.cpp
    src/grbl/TextToGcode/SingleStrokeConverter.cpp
    src/grbl/GraphicsDesign/Entity.cpp
    src/grbl/GraphicsDesign/Shape.cpp
    src/grbl/GraphicsDesign/Document.cpp
    src/grbl/GraphicsDesign/CommandStack.cpp
    src/grbl/GraphicsDesign/ShapeModel.cpp
    src/grbl/GraphicsDesign/LineEntity.cpp
    src/grbl/GraphicsDesign/PolylineEntity.cpp
    src/grbl/GraphicsDesign/SplineEntity.cpp
    src/grbl/GraphicsDesign/GroupEntity.cpp
    src/grbl/GraphicsDesign/LinearDimensionEntity.cpp
    src/grbl/GraphicsDesign/AngularDimensionEntity.cpp
    src/grbl/GraphicsDesign/RadiusDimensionEntity.cpp
    src/grbl/GraphicsDesign/RectangleEntity.cpp
    src/grbl/GraphicsDesign/CircleEntity.cpp
    src/grbl/GraphicsDesign/ArcEntity.cpp
    src/grbl/GraphicsDesign/TextEntity.cpp
    src/grbl/GraphicsDesign/GeometryUtils.cpp
    src/grbl/GraphicsDesign/TrimModification.cpp
    src/grbl/GraphicsDesign/LineShape.cpp
    src/grbl/GraphicsDesign/RectShape.cpp
    src/grbl/GraphicsDesign/CircleShape.cpp
    src/grbl/GraphicsDesign/ArcShape.cpp
    src/grbl/GraphicsDesign/Tools/SelectTool.cpp
    src/grbl/GraphicsDesign/Tools/LineTool.cpp
    src/grbl/GraphicsDesign/Tools/PolylineTool.cpp
    src/grbl/GraphicsDesign/Tools/DimensionTool.cpp
    src/grbl/GraphicsDesign/Tools/AngularDimensionTool.cpp
    src/grbl/GraphicsDesign/Tools/RadiusDimensionTool.cpp
    src/grbl/GraphicsDesign/Tools/MoveTool.cpp
    src/grbl/GraphicsDesign/Tools/CopyTool.cpp
    src/grbl/GraphicsDesign/Tools/RotateTool.cpp
    src/grbl/GraphicsDesign/Tools/TrimExtendTool.cpp
    src/grbl/GraphicsDesign/Tools/TrimTool.cpp
    src/grbl/GraphicsDesign/Tools/ChamferTool.cpp
    src/grbl/GraphicsDesign/Tools/FilletTool.cpp
    src/grbl/GraphicsDesign/Tools/OffsetTool.cpp
    src/grbl/GraphicsDesign/Tools/MeasureTool.cpp
    src/grbl/GraphicsDesign/Tools/SplineTool.cpp
    src/grbl/GraphicsDesign/Tools/RectangleTool.cpp
    src/grbl/GraphicsDesign/Tools/CircleTool.cpp
    src/grbl/GraphicsDesign/Tools/ArcTool.cpp
    src/grbl/GraphicsDesign/Tools/TextTool.cpp
    src/grbl/GraphicsDesign/VectorFontCatalog.cpp
    src/grbl/ImageVectorizer.cpp
    src/grbl/DxfParser.cpp
)

set(GRBL_HEADERS
    src/grbl/ToolLibrary.h
    src/grbl/ToolChangeMacroCompiler.h
    src/grbl/MachineState.h
    src/grbl/GrblParser.h
    src/grbl/SerialManager.h
    src/grbl/GcodeStreamer.h
    src/grbl/GcodeParser.h
    src/grbl/FileManager.h
    src/grbl/GrblConstants.h
    src/grbl/types.h
    src/grbl/TextToGcode/VectorFontParser.h
    src/grbl/TextToGcode/HersheyFontParser.h
    src/grbl/TextToGcode/SvgFontParser.h
    src/grbl/TextToGcode/HpglFontParser.h
    src/grbl/TextToGcode/JsonFontParser.h
    src/grbl/TextToGcode/ShxFontParser.h
    src/grbl/TextToGcode/VectorFontLoader.h
    src/grbl/TextToGcode/TextToGcodeGenerator.h
    src/grbl/TextToGcode/SingleStrokeConverter.h
    src/grbl/GraphicsDesign/Entity.h
    src/grbl/GraphicsDesign/Shape.h
    src/grbl/GraphicsDesign/Document.h
    src/grbl/GraphicsDesign/CommandStack.h
    src/grbl/GraphicsDesign/ShapeModel.h
    src/grbl/GraphicsDesign/LineEntity.h
    src/grbl/GraphicsDesign/PolylineEntity.h
    src/grbl/GraphicsDesign/GroupEntity.h
    src/grbl/GraphicsDesign/LinearDimensionEntity.h
    src/grbl/GraphicsDesign/AngularDimensionEntity.h
    src/grbl/GraphicsDesign/RadiusDimensionEntity.h
    src/grbl/GraphicsDesign/Layer.h
    src/grbl/GraphicsDesign/RectangleEntity.h
    src/grbl/GraphicsDesign/CircleEntity.h
    src/grbl/GraphicsDesign/ArcEntity.h
    src/grbl/GraphicsDesign/TextEntity.h
    src/grbl/GraphicsDesign/GeometryUtils.h
    src/grbl/GraphicsDesign/TrimModification.h
    src/grbl/GraphicsDesign/LineShape.h
    src/grbl/GraphicsDesign/RectShape.h
    src/grbl/GraphicsDesign/CircleShape.h
    src/grbl/GraphicsDesign/ArcShape.h
    src/grbl/GraphicsDesign/Tools/Tool.h
    src/grbl/GraphicsDesign/Tools/SelectTool.h
    src/grbl/GraphicsDesign/Tools/LineTool.h
    src/grbl/GraphicsDesign/Tools/PolylineTool.h
    src/grbl/GraphicsDesign/Tools/DimensionTool.h
    src/grbl/GraphicsDesign/Tools/AngularDimensionTool.h
    src/grbl/GraphicsDesign/Tools/RadiusDimensionTool.h
    src/grbl/GraphicsDesign/Tools/MoveTool.h
    src/grbl/GraphicsDesign/Tools/CopyTool.h
    src/grbl/GraphicsDesign/Tools/RotateTool.h
    src/grbl/GraphicsDesign/Tools/RectangleTool.h
    src/grbl/GraphicsDesign/Tools/CircleTool.h
    src/grbl/GraphicsDesign/Tools/ArcTool.h
    src/grbl/GraphicsDesign/Tools/TextTool.h
    src/grbl/GraphicsDesign/VectorFontCatalog.h
    src/grbl/ImageVectorizer.h
    src/grbl/DxfParser.h
)

# --- UI ---
set(UI_SOURCES
    src/ui/ToolsDialog.cpp
    src/ui/MainWindow.cpp
    src/ui/DockPanel.cpp
    src/ui/CustomTitleBar.cpp
    src/ui/ToolpathCanvas.cpp
    src/ui/GcodeListWidget.cpp
    src/ui/ToolpathPanel.cpp
    src/ui/TextToGcodeDialog.cpp
    src/ui/panels/ConnectionPanel.cpp
    src/ui/panels/StatusPanel.cpp
    src/ui/panels/ControlPanel.cpp
    src/ui/panels/JogPanel.cpp
    src/ui/panels/ConsolePanel.cpp
    src/ui/panels/SettingsPanel.cpp
    src/ui/panels/GraphicsDesignPanel.cpp
    src/ui/panels/LayerListDelegate.cpp
    src/ui/panels/EntityTransform.cpp
    src/ui/panels/TransformPanel.cpp
    src/ui/panels/LayerPanel.cpp
    src/ui/panels/ToolBarWidget.cpp
    src/ui/DesignCanvas.cpp
    src/ui/canvas/ToolManager.cpp
    src/ui/canvas/CanvasDrawing.cpp
    src/ui/canvas/CanvasSnap.cpp
    src/ui/canvas/EntityClipboard.cpp
    src/ui/GraphicsDesignDialog.cpp
    src/ui/LineNumberPlainTextEdit.cpp
    src/ui/panels/PropertyPages/PropertyPage.cpp
    src/ui/panels/PropertyPages/LinePage.cpp
    src/ui/panels/PropertyPages/RectanglePage.cpp
    src/ui/panels/PropertyPages/CirclePage.cpp
    src/ui/panels/PropertyPages/ArcPage.cpp
    src/ui/panels/PropertyPages/DimensionPage.cpp
    src/ui/panels/PropertyPages/AngularDimensionPage.cpp
    src/ui/panels/PropertyPages/RadiusDimensionPage.cpp
    src/ui/panels/PropertyPages/SplinePage.cpp
    src/ui/panels/PropertyPages/PolylinePage.cpp
    src/ui/panels/PropertyPages/EmptyPage.cpp
)

set(UI_FILES
    src/ui/MainWindow.ui
    src/ui/TextToGcodeDialog.ui
    src/ui/GraphicsDesignDialog.ui
    src/ui/panels/StatusPanel.ui
    src/ui/panels/JogPanel.ui
    src/ui/panels/ConnectionPanel.ui
    src/ui/panels/ControlPanel.ui
)

set(UI_HEADERS
    src/ui/ToolsDialog.h
    src/ui/MainWindow.h
    src/ui/DockPanel.h
    src/ui/CustomTitleBar.h
    src/ui/ToolpathCanvas.h
    src/ui/GcodeListWidget.h
    src/ui/ToolpathPanel.h
    src/ui/TextToGcodeDialog.h
    src/ui/panels/ConnectionPanel.h
    src/ui/panels/StatusPanel.h
    src/ui/panels/ControlPanel.h
    src/ui/panels/JogPanel.h
    src/ui/panels/ConsolePanel.h
    src/ui/panels/SettingsPanel.h
    src/ui/panels/GraphicsDesignPanel.h
    src/ui/panels/TransformPanel.h
    src/ui/panels/LayerPanel.h
    src/ui/panels/ToolBarWidget.h
    src/ui/DesignCanvas.h
    src/ui/canvas/ToolManager.h
    src/ui/canvas/CoordinateTransform.h
    src/ui/canvas/CanvasDrawing.h
    src/ui/canvas/CanvasSnap.h
    src/ui/canvas/EntityClipboard.h
    src/ui/GraphicsDesignDialog.h
    src/ui/LineNumberPlainTextEdit.h
    src/ui/panels/PropertyPages/PropertyPage.h
    src/ui/panels/PropertyPages/LinePage.h
    src/ui/panels/PropertyPages/RectanglePage.h
    src/ui/panels/PropertyPages/CirclePage.h
    src/ui/panels/PropertyPages/ArcPage.h
    src/ui/panels/PropertyPages/DimensionPage.h
    src/ui/panels/PropertyPages/AngularDimensionPage.h
    src/ui/panels/PropertyPages/RadiusDimensionPage.h
    src/ui/panels/PropertyPages/SplinePage.h
    src/ui/panels/PropertyPages/PolylinePage.h
    src/ui/panels/PropertyPages/EmptyPage.h
)

set(RESOURCES
    resources/resources.qrc
)

# Compile each domain once; target interfaces express its real dependencies.
add_library(GcodeModel STATIC src/grbl/GcodeParser.cpp)
target_include_directories(GcodeModel PUBLIC src)
target_link_libraries(GcodeModel PUBLIC Qt6::Core Qt6::Gui)

add_library(PlotModel STATIC src/plotting/PlotJob.cpp)
target_include_directories(PlotModel PUBLIC src)
target_link_libraries(PlotModel PUBLIC Qt6::Core)

set(CAD_SOURCES ${GRBL_SOURCES})
list(FILTER CAD_SOURCES INCLUDE REGEX "/(GraphicsDesign|TextToGcode)/")
set(CAD_TOOL_SOURCES ${CAD_SOURCES})
list(FILTER CAD_TOOL_SOURCES INCLUDE REGEX "/Tools/")
list(FILTER CAD_SOURCES EXCLUDE REGEX "/Tools/")
add_library(CadCore STATIC ${CAD_SOURCES})
target_include_directories(CadCore PUBLIC src PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(CadCore PUBLIC GcodeModel Qt6::Core Qt6::Gui Qt6::Core5Compat PRIVATE ${OpenCV_LIBS})

add_library(CadTools STATIC ${CAD_TOOL_SOURCES})
target_include_directories(CadTools PUBLIC src)
target_link_libraries(CadTools PUBLIC CadCore Qt6::Widgets)

add_library(PlotEngine STATIC
    src/grbl/ToolLibrary.cpp src/grbl/ToolChangeMacroCompiler.cpp
    src/plotting/PlotJobBuilder.cpp src/plotting/PlotJobTransform.cpp src/plotting/RackJobValidator.cpp)
target_include_directories(PlotEngine PUBLIC src)
target_link_libraries(PlotEngine PUBLIC CadCore PlotModel GcodeModel Qt6::Core Qt6::Gui)

add_library(GrblBackend STATIC
    src/grbl/GrblTransport.h src/grbl/SerialManager.cpp src/grbl/GcodeStreamer.cpp
    src/grbl/GrblParser.cpp src/grbl/MachineState.cpp src/grbl/FileManager.cpp)
target_include_directories(GrblBackend PUBLIC src)
target_link_libraries(GrblBackend PUBLIC PlotModel GcodeModel Qt6::Core Qt6::Gui Qt6::SerialPort)

add_library(ImportServices STATIC src/grbl/ImageVectorizer.cpp src/grbl/DxfParser.cpp)
target_include_directories(ImportServices PUBLIC src PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ImportServices PUBLIC CadCore PRIVATE ${OpenCV_LIBS})

add_library(ApplicationServices STATIC ${APP_SOURCES})
target_include_directories(ApplicationServices PUBLIC src)
target_link_libraries(ApplicationServices PUBLIC GrblBackend PlotEngine)

add_library(GrblUi STATIC ${UI_SOURCES} ${UI_HEADERS} ${UI_FILES})
target_sources(GrblUi PRIVATE src/ui/RackJobStartDialog.cpp src/ui/RackJobStartDialog.h)
target_include_directories(GrblUi PUBLIC src)
target_link_libraries(GrblUi PUBLIC ApplicationServices ImportServices CadTools Qt6::Widgets
    Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent)

add_executable(${PROJECT_NAME} WIN32 src/main.cpp src/App.cpp ${RESOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE GrblUi)

if(MSVC)
    foreach(target GcodeModel PlotModel CadCore CadTools PlotEngine GrblBackend ImportServices ApplicationServices GrblUi)
        target_compile_options(${target} PRIVATE /MP)
        target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
    endforeach()
endif()

if(WIN32 AND EXISTS "${Qt6_DIR}/../../../bin/windeployqt.exe")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe" --no-compiler-runtime
            --dir "$<TARGET_FILE_DIR:${PROJECT_NAME}>" "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Deploying Qt dependencies...")
endif()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/bigfonts"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/bigfonts")
if(WIN32)
    file(GLOB OPENCV_DLLS "${OpenCV_DIR}/../bin/opencv_world*.dll")
    foreach(DLL ${OPENCV_DLLS})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/")
    endforeach()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(TestGcodeParser tests/test_gcode_parser.cpp)
    target_link_libraries(TestGcodeParser PRIVATE GcodeModel CadCore)
    add_executable(TestShxDebug tests/test_shx_debug.cpp)
    target_link_libraries(TestShxDebug PRIVATE CadCore)
    add_executable(TestCanvasSnap tests/test_canvas_snap.cpp src/ui/canvas/CanvasSnap.cpp
        src/ui/panels/EntityTransform.cpp)
    target_link_libraries(TestCanvasSnap PRIVATE CadTools PlotEngine)
    add_executable(TestPenAssignment tests/test_pen_assignment.cpp ${RESOURCES})
    target_link_libraries(TestPenAssignment PRIVATE GrblUi)
    add_executable(TestToolChangeMacro tests/test_tool_change_macro.cpp)
    target_link_libraries(TestToolChangeMacro PRIVATE PlotEngine)
    add_executable(TestArchitectureRegression tests/test_architecture_regression.cpp ${RESOURCES})
    target_link_libraries(TestArchitectureRegression PRIVATE GrblUi)
    add_executable(TestJobController tests/test_job_controller.cpp)
    target_link_libraries(TestJobController PRIVATE GrblBackend)
    add_executable(TestPlotPipeline tests/test_plot_pipeline.cpp)
    target_link_libraries(TestPlotPipeline PRIVATE PlotEngine)
    add_executable(TestAutomaticRack tests/test_automatic_rack.cpp)
    target_link_libraries(TestAutomaticRack PRIVATE PlotEngine GrblBackend)
    add_executable(TestRackExecution tests/test_rack_execution.cpp)
    target_link_libraries(TestRackExecution PRIVATE PlotEngine GrblBackend)
    add_executable(TestAutomaticRackUi tests/test_automatic_rack_ui.cpp ${RESOURCES})
    target_link_libraries(TestAutomaticRackUi PRIVATE GrblUi)
    add_executable(TestGcodePenColors tests/test_gcode_pen_colors.cpp ${RESOURCES})
    target_link_libraries(TestGcodePenColors PRIVATE GrblUi)
    add_executable(TestToolpathPenColors tests/test_toolpath_pen_colors.cpp ${RESOURCES})
    target_link_libraries(TestToolpathPenColors PRIVATE GrblUi)
    add_executable(TestToolFilter tests/test_tool_filter.cpp ${RESOURCES})
    target_link_libraries(TestToolFilter PRIVATE GrblUi)
    add_executable(TestToolLibrary tests/test_tool_library.cpp ${RESOURCES})
    target_link_libraries(TestToolLibrary PRIVATE GrblUi)
    foreach(target TestGcodeParser TestCanvasSnap TestPenAssignment TestToolChangeMacro TestArchitectureRegression TestJobController TestPlotPipeline TestAutomaticRack TestRackExecution TestAutomaticRackUi TestGcodePenColors TestToolpathPenColors TestToolFilter TestToolLibrary)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
        if(WIN32)
            set_tests_properties(${target} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${Qt6_DIR}/../../../bin;PATH=path_list_prepend:${OpenCV_DIR}/../bin")
            set_property(TEST ${target} APPEND PROPERTY ENVIRONMENT
                "QT_QPA_PLATFORM_PLUGIN_PATH=${Qt6_DIR}/../../../plugins/platforms")
        endif()
    endforeach()
endif()
