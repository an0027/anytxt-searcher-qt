@echo off
cd /d "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build"

set PATH=C:\Qt6\6.11.0\msvc2022_64\bin;C:\tools\vcpkg\installed\x64-windows\bin;%PATH%
set QT_LOGGING_RULES=*.debug=true

echo Starting app in background...
start /B anytxt-searcher.exe 2> debug_stderr.txt
set PID=%ERRORLEVEL%
echo PID=%PID%, waiting 5 seconds...
ping -n 5 127.0.0.1 >nul

echo Killing app...
taskkill /f /im anytxt-searcher.exe >nul 2>&1
timeout /t 1 /nobreak >nul

echo === STDOUT/STDERR capture ===
type debug_stderr.txt

if exist crash_log.txt (
    echo.
    echo === CRASH LOG ===
    type crash_log.txt
)
