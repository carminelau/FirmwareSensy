@echo off
setlocal

where py >nul 2>&1
if %errorlevel%==0 (
    py -3 scripts\build_matrix.py --check %*
) else (
    python scripts\build_matrix.py --check %*
)

exit /b %errorlevel%
