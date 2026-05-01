@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1

set QTDIR=C:\Qt6\6.11.0\msvc2022_64

echo === Clean build with MSVC Qt paths ===

cmake -S C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt -B C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build ^
  -DQt6_DIR=%QTDIR%\lib\cmake\Qt6 ^
  -DQt6Core_DIR=%QTDIR%\lib\cmake\Qt6Core ^
  -DQt6Gui_DIR=%QTDIR%\lib\cmake\Qt6Gui ^
  -DQt6Widgets_DIR=%QTDIR%\lib\cmake\Qt6Widgets ^
  -DQt6Concurrent_DIR=%QTDIR%\lib\cmake\Qt6Concurrent ^
  -DQt6Core5Compat_DIR=%QTDIR%\lib\cmake\Qt6Core5Compat ^
  -DQt6EntryPointPrivate_DIR=%QTDIR%\lib\cmake\Qt6EntryPointPrivate ^
  -DQt6CoreTools_DIR=%QTDIR%\lib\cmake\Qt6CoreTools ^
  -DQt6GuiTools_DIR=%QTDIR%\lib\cmake\Qt6GuiTools ^
  -DQt6WidgetsTools_DIR=%QTDIR%\lib\cmake\Qt6WidgetsTools ^
  -DCMAKE_BUILD_TYPE=Release 2>&1

echo.
echo === Build ===
cmake --build C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build --config Release 2>&1
