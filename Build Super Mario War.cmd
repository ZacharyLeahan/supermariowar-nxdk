@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\windows-bootstrap.ps1" %*
if errorlevel 1 (
  echo.
  echo Build failed. Read the message above, then press any key to close.
  pause >nul
  exit /b 1
)
echo.
echo Build complete. Press any key to close.
pause >nul
