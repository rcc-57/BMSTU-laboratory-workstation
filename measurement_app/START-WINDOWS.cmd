@echo off
setlocal EnableExtensions DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
echo Building the application and testing the protocol...
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%CD%\build-windows.ps1"
if errorlevel 1 goto failed
set "APP=%CD%\..\build\measurement-windows\Release\measurement_control.exe"

echo Checking the desktop window. It should close automatically after 120 frames.
"%APP%" --smoke-test
if errorlevel 1 goto failed
echo Starting Measurement Instrument Control. Select the meter port and press Connect.
"%APP%"
if errorlevel 1 goto failed
popd
exit /b 0

:failed
echo See README_how_to_start.md. Keep the error above for the bench report.
pause
popd
exit /b 1
