@echo off

rem Builds this project for x86 and x64, and collects a release in build_final\. The script is the one of API Interceptor
rem (see its header for the arguments and for what it does), pointed at this project.
call "%~dp0deps\APIInterceptor\build.bat" --source-dir "%~dp0." %*
exit /b %errorlevel%
