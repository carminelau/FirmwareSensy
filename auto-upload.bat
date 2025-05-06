@echo off
setlocal enabledelayedexpansion

set PLATFORMIO="C:\Users\Admin\.platformio\penv\Scripts\platformio.exe"
set ENV=sensy_2024_V4_black
set PORT=COM9
set COUNT=25

for /L %%i in (1,1,%COUNT%) do (
    echo -----------------------------------
    echo Upload %%i di %COUNT% su %PORT%...
    %PLATFORMIO% run --target upload --environment %ENV% --upload-port %PORT%
    if errorlevel 1 (
        echo ❌ Upload %%i fallito.
    ) else (
        echo ✅ Upload %%i riuscito.
    )
    echo.
    set /p CONT=Collega il prossimo dispositivo e premi INVIO per continuare...
)

echo Tutti gli upload completati.
pause
