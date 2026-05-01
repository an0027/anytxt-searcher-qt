@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=C:\Qt6\6.11.0\msvc2022_64\bin;C:\tools\vcpkg\installed\x64-windows\bin;%PATH%
cd /d "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build"

if not exist anytxt-searcher.exe (
    echo ERROR: exe not found
    exit /b 1
)

echo ===== Starting app (3 sec test) =====
start /B anytxt-searcher.exe >nul 2>&1
set PID=!ERRORLEVEL!
timeout /t 3 /nobreak >nul

tasklist /fi "imagename eq anytxt-searcher.exe" 2>nul | find /i "anytxt-searcher" >nul
if %errorlevel% equ 0 (
    echo [PASS] Process started and stayed alive for 3 seconds
    taskkill /f /im anytxt-searcher.exe >nul 2>&1
) else (
    echo [FAIL] Process exited immediately
    exit /b 1
)
echo ===== Test complete =====
