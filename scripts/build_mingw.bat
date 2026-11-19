@echo off
rem Builds multi-tool.exe with MinGW-w64 (MSYS2: pacman -S mingw-w64-x86_64-gcc make).  Output: multi-tool.exe
setlocal
pushd "%~dp0.."
where gcc >nul 2>nul || (echo gcc not found in PATH. Open the "MSYS2 MinGW x64" shell or add mingw64\bin to PATH. & popd & exit /b 1)
where mingw32-make >nul 2>nul && (set "MK=mingw32-make") || (set "MK=make")
%MK% CC=gcc WINDRES=windres
set "RC=%errorlevel%"
popd
if not "%RC%"=="0" exit /b %RC%
echo.
echo Done: multi-tool.exe
endlocal
