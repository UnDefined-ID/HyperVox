@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_and_build_windows.ps1"
if errorlevel 1 (
  echo.
  echo Build failed. Run this .bat from an Administrator Command Prompt/PowerShell.
  pause
  exit /b 1
)
pause
