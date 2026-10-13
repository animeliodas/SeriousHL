@echo off
rem SeriousHL: removes the plugin (Bin\Plugins\SeriousHL.dll) and its desktop shortcuts; Classics Patch stays.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" -Uninstall %*
pause
