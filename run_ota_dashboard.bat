@echo off
setlocal

where py >nul 2>&1
if %errorlevel%==0 (
    py -3 tools\ota_dashboard.py --open
) else (
    python tools\ota_dashboard.py --open
)
