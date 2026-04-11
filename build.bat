@echo off
REM GRBL G-Code Sender - Build Script for VS 2026 + Qt 6.11.0

call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

set CMAKE_PREFIX_PATH=C:/Qt/6.11.0/msvc2022_64
set PATH=C:\Qt\6.11.0\msvc2022_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%

cd /d D:\projects\github\grbl_cpp

if "%1"=="clean" (
    echo Cleaning build directory...
    if exist build rd /s /q build
)

if not exist build (
    echo Configuring CMake...
    cmake -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH%
    if errorlevel 1 (
        echo CMAKE CONFIGURE FAILED
        pause
        exit /b 1
    )
)

echo Building Release...
cmake --build build --config Release
if errorlevel 1 (
    echo BUILD FAILED
    pause
    exit /b 1
)

echo.
echo Build successful!
echo Executable: build\bin\Release\GrblSender.exe
pause