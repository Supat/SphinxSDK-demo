@echo off
REM No-console launcher: runs the app under pythonw.exe so no console window
REM stays open. If the app does NOT appear, run run_app.bat instead -- it keeps
REM the console open and shows the error.
cd /d "%~dp0"
set "PYWEXE=C:\Program Files\Python312\pythonw.exe"
if not exist "%PYWEXE%" set "PYWEXE=pythonw"
start "" "%PYWEXE%" app.py
