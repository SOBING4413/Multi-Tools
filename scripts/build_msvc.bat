@echo off
rem Builds multi-tool.exe with Visual Studio 2022 (x64 Release).  Output: bin\x64\Release\multi-tool.exe
setlocal
set "ROOT=%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo vswhere.exe not found. Install Visual Studio 2022 with the "Desktop development with C++" workload,
  echo or open multitool.sln in Visual Studio and press Ctrl+Shift+B.
  exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
  echo MSBuild not found.
  exit /b 1
)
"%MSBUILD%" "%ROOT%\multitool.sln" /p:Configuration=Release /p:Platform=x64 /m /nologo /v:m
if errorlevel 1 exit /b 1
echo.
echo Done: %ROOT%\bin\x64\Release\multi-tool.exe
endlocal
