cmake_minimum_required(VERSION 3.16)
project(GrblSender VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)
find_package(Qt6CorePrivate REQUIRED)

qt_standard_project_setup()

set(GRBL_SOURCES
    src/grbl/MachineState.cpp
    src/grbl/GrblParser.cpp
    src/grbl/SerialManager.cpp
    src/grbl/GcodeParser.cpp
    src/grbl/FileManager.cpp
)

set(GRBL_HEADERS
    src/grbl/MachineState.h
    src/grbl/GrblParser.h
    src/grbl/SerialManager.h
    src/grbl/GcodeParser.h
    src/grbl/FileManager.h
    src/grbl/types.h
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
)

set(UI_HEADERS
    src/ui/MainWindow.h
)

set(SOURCES
    src/main.cpp
    src/App.cpp
    ${GRBL_SOURCES}
    ${UI_SOURCES}
)

set(HEADERS
    src/App.h
    ${GRBL_HEADERS}
    ${UI_HEADERS}
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::CorePrivate
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    target_link_libraries(${PROJECT_NAME} PRIVATE setupapi)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
    target_link_options(${PROJECT_NAME} PRIVATE /DEBUG)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
        PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
        COMPILE_PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
        COMPILE_PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    )
endif()

if(WIN32 AND EXISTS "${Qt6_DIR}/../../../bin/windeployqt.exe")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-compiler-runtime
            --dir "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            "$<TARGET_FILE:${PROJECT_NAME}>"
        COMMENT "Deploying Qt dependencies..."
    )
endif()