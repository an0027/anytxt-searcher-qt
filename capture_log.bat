@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=C:\Qt6\6.11.0\msvc2022_64\bin;C:\tools\vcpkg\installed\x64-windows\bin;%PATH%
cd /d "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build"
set QT_LOGGING_RULES=*.debug=true
anytxt-searcher.exe > debug_console.txt 2>&1
echo Exit code: %errorlevel% >> debug_console.txt
