@echo off
setlocal
py -3.14 -m voxprint_lucent %*
exit /b %ERRORLEVEL%
