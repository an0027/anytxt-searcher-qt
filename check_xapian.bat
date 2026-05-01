@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
echo === Xapian lib ===
dumpbin /headers "C:\tools\vcpkg\installed\x64-windows\lib\xapian.lib" 2>&1 | findstr /i "machine"
echo.
echo === Xapian DLL ===
dumpbin /headers "C:\tools\vcpkg\installed\x64-windows\bin\xapian-30.dll" 2>&1 | findstr /i "machine"
echo.
echo === Our EXE ===
dumpbin /headers "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build\anytxt-searcher.exe" 2>&1 | findstr /i "machine subsystem"
echo.
echo === Xapian DLL exports ===
dumpbin /exports "C:\tools\vcpkg\installed\x64-windows\bin\xapian-30.dll" 2>&1 | findstr /i "MatchAll" | head -5
echo.
echo === Verify DLL exists ===
dir "C:\tools\vcpkg\installed\x64-windows\bin\xapian-30.dll"
