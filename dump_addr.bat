@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set MAP="C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build\anytxt-searcher.map"

echo === Segment info ===
findstr /i "start\|length\|group\|segment" %MAP%
echo.
echo === Text segment entries sorted by address ===
findstr /n "." %MAP% | findstr /i "0003:" | findstr /i "xapian_searcher search get_mset enquire parse_query convertToDocument"
echo.
echo === Functions around offset 0xe900 ===
findstr /n "." %MAP% | findstr /i "0003:e[0-9a-f]" <&0
echo.
echo === All functions in 0003 section ===
findstr /n "." %MAP% | findstr /i "0003:" | findstr /i "\.text$" <&0
