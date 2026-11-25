@echo off
setlocal

set ROOT=%~dp0..\..
set LOG=%~dp0build.txt
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

echo [1/2] Configuring project with CMake...
cmake -S "%ROOT%" -B "%ROOT%\build" -G Ninja -DCMAKE_CXX_COMPILER=g++ -DCMAKE_MAKE_PROGRAM=ninja -DFALCON_NETWORK_BUILD_EXAMPLES=ON >> "%LOG%" 2>&1
if errorlevel 1 (
    echo [ERROR] CMake configuration failed. See %LOG% for details.
    exit /b 1
)

echo [2/2] Building FalconProxy...
cmake --build "%ROOT%\build" --target FalconProxy >> "%LOG%" 2>&1
if errorlevel 1 (
    echo [ERROR] Build failed. Full output saved to %LOG%.
    exit /b 1
)

echo Build succeeded: build\examples\Proxy\FalconProxy.exe
endlocal
