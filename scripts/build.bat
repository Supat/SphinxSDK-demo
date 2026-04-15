@echo off
setlocal
rem Build ConsoleDemo_FPN. First arg = config (Release|Debug), default Release.

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [ERROR] vswhere.exe not found. Install Visual Studio 2022.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do set "VS_ROOT=%%i"
set "MSBUILD=%VS_ROOT%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
  echo [ERROR] MSBuild.exe not found under %VS_ROOT%
  exit /b 1
)

set "REPO=%~dp0..\"
"%MSBUILD%" "%REPO%ConsoleDemo_FPN\ConsoleDemo.sln" /p:Configuration=%CONFIG% /p:Platform=x64 /m
if errorlevel 1 exit /b 1

set "OUT=%REPO%ConsoleDemo_FPN\x64\%CONFIG%\"
echo.
echo Build OK. Copying ONNX models next to the binary...
copy /Y "%REPO%ConsoleDemo_FPN\hand_landmark_full.onnx" "%OUT%" >nul
copy /Y "%REPO%ConsoleDemo_FPN\pose_landmark_full.onnx" "%OUT%" >nul
echo Run: "%OUT%ConsoleDemo.exe"
endlocal
