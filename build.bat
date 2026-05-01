@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

set QT_MSVC=C:\Qt6\6.11.0\msvc2022_64

echo === Configuring with MSVC Qt only ===
cmake -S C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt -B C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build ^
  -DCMAKE_PREFIX_PATH=%QT_MSVC% ^
  -DQt6_DIR=%QT_MSVC%\lib\cmake\Qt6 2>&1

echo.
echo === Build ===
cmake --build C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build --config Release 2>&1
