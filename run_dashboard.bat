@echo off
setlocal
cd /d "%~dp0"
python python\dashboard_live.py
if errorlevel 1 (
    py python\dashboard_live.py
)
