@echo off
"%~dp0python\python.exe" -m voxprint_lucent.tools_host %*
exit /b %ERRORLEVEL%
