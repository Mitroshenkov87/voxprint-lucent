@echo off
setlocal
py -3.15 -m voxprint_lucent %*
exit /b %ERRORLEVEL%
