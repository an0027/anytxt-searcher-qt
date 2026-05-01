@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
echo === Release dependencies ===
dumpbin /dependents "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build\anytxt-searcher.exe" | findstr /i "\.dll"
