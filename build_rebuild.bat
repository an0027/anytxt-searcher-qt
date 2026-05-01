@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set PKG_CONFIG_PATH=C:/tools/vcpkg/installed/x64-windows/lib/pkgconfig
set PATH=C:\Qt6\Tools\Ninja;C:\Qt6\Tools\CMake_64\bin;C:\tools\vcpkg\installed\x64-windows\bin;%PATH%

cd /d "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build"

echo === Clean build dir ===
if exist * ( if exist CMakeCache.txt del /f CMakeCache.txt )

echo === CMake Configure ===
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt6/6.11.0/msvc2022_64;C:/tools/vcpkg/installed/x64-windows" -DCMAKE_CXX_FLAGS="/EHsc"
if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo === Ninja Build ===
ninja -j4
if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo === Build Complete ===
if exist anytxt-searcher.exe (
    dir anytxt-searcher.exe
) else (
    echo ERROR: anytxt-searcher.exe not found!
    exit /b 1
)
