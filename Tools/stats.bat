@echo off
REM ===============================================
REM  C++ Project Statistics Tool
REM  Usage: stats.bat [path]
REM
REM  Examples:
REM    stats.bat              (analyzes parent directory)
REM    stats.bat .            (analyzes current directory)
REM    stats.bat C:\MyProject (analyzes specified path)
REM ===============================================

setlocal

set "SCRIPT_DIR=%~dp0"
set "TARGET_PATH=%~1"
set "EXCLUDE_DIRS=%~2"

REM If no path specified, use parent directory (project root)
if "%TARGET_PATH%"=="" set "TARGET_PATH=%SCRIPT_DIR%.."

if "%EXCLUDE_DIRS%"=="" set "EXCLUDE_DIRS=external,vendor,third_party,node_modules,build,out,x64,Debug,Release,.git"

powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%project_stats.ps1" -Path "%TARGET_PATH%" -ExcludeDirs "%EXCLUDE_DIRS%"

endlocal
pause
