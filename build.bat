@echo off
setlocal

set LOG=build.txt
set PARALLEL_ARGS=

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="--parallel" (
    if "%~2"=="" (
        echo [ERROR] --parallel needs a job count, for example: build.bat --parallel 4
        exit /b 1
    )
    set PARALLEL_ARGS=--parallel %~2
    shift
    shift
    goto parse_args
)
echo [ERROR] Unknown argument: %~1
exit /b 1
:args_done

break > "%LOG%"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found in PATH.
    exit /b 1
)

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found in PATH. Install MSYS2 UCRT64 and add it to PATH.
    exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
    echo [ERROR] ninja not found in PATH. Install MSYS2 UCRT64 and add it to PATH.
    exit /b 1
)

echo [2/3] Configuring project with CMake...
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_MAKE_PROGRAM=ninja >> "%LOG%" 2>&1
if errorlevel 1 (
    echo [ERROR] CMake configuration failed. See %LOG% for details.
    type "%LOG%"
    exit /b 1
)

echo [3/3] Building FalconServer...
cmake --build build --config Release %PARALLEL_ARGS% >> "%LOG%" 2>&1
set BUILD_RESULT=%errorlevel%

type "%LOG%"

if not "%BUILD_RESULT%"=="0" (
    echo [ERROR] Build failed. Full output saved to %LOG%.
    exit /b 1
)

if not exist build\Falcon.Server\FalconServer.exe (
    echo [ERROR] Build reported success but FalconServer.exe was not produced. See %LOG%.
    exit /b 1
)

echo.
echo Build succeeded: build\Falcon.Server\FalconServer.exe
echo Full build log saved to %LOG%
endlocal
