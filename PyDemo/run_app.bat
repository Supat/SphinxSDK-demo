@echo off
REM Launch the PySide6 wrist-angle app by double-clicking from Explorer.
REM Pins the working directory to this folder and pauses on error so any
REM message stays visible.
cd /d "%~dp0"
set "PYEXE=C:\Program Files\Python312\python.exe"
if not exist "%PYEXE%" set "PYEXE=py"
echo Launching app.py with %PYEXE% ...
"%PYEXE%" app.py
if errorlevel 1 pause
