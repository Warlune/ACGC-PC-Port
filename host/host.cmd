@echo off
setlocal
cd /d "%~dp0"
if defined ACO_PYTHON (
  "%ACO_PYTHON%" -m aco_host %*
) else (
  py -3 -m aco_host %*
)
exit /b %ERRORLEVEL%
