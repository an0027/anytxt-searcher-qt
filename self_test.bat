@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=C:\Qt6\6.11.0\msvc2022_64\bin;C:\tools\vcpkg\installed\x64-windows\bin;%PATH%

echo ===== Running self-test =====
del "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build\test_result.txt" 2>nul

"C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build\anytxt-searcher.exe" --self-test
echo Exit code: %errorlevel%

echo.
echo ===== Test results =====
type "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build\test_result.txt"
