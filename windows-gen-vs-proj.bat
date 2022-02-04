echo off
echo select visual studio version:
echo 1) vs2015
echo 2) vs2017
echo 3) vs2019
echo 4) vs2022
set /p vs_version="version: "
call vendor\\premake5.exe %vs_version%
pause