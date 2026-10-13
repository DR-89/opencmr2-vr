@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-PCVR.ps1" %*
set "OpenCMR2ExitCode=%ERRORLEVEL%"
if not "%OpenCMR2ExitCode%"=="0" pause
exit /b %OpenCMR2ExitCode%
