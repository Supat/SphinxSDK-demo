@echo off
rem Convenience wrapper that runs setup.ps1 with the right execution policy.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" %*
