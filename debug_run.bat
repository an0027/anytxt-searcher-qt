@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=C:\Qt6\6.11.0\msvc2022_64\bin;C:\tools\vcpkg\installed\x64-windows\bin;%PATH%
cd /d "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build"

echo ===== Launching with debug logging =====
echo Logs will be output to console below
echo Press Ctrl+C to exit
echo ========================================
set QT_LOGGING_RULES=*.debug=true
anytxt-searcher.exe
echo ===== App exited with code: %errorlevel% =====
pause
