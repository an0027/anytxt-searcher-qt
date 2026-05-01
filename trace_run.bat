@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build"
set PATH=C:\Qt6\6.11.0\msvc2022_64\bin;C:\tools\vcpkg\installed\x64-windows\bin;%PATH%

echo Launching app...
start "" anytxt-searcher.exe

REM Wait 6 seconds for auto-diagnostic to trigger
ping -n 6 127.0.0.1 >nul

echo Cleaning up...
taskkill /f /im anytxt-searcher.exe >nul 2>&1
ping -n 2 127.0.0.1 >nul

echo.
echo === search_trace.txt ===
if exist search_trace.txt (
    type search_trace.txt
) else (
    echo (file not found)
)
echo.
echo === crash_log.txt ===
if exist crash_log.txt (
    type crash_log.txt
) else (
    echo (file not found)
)
