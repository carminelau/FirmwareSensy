@echo off
setlocal enabledelayedexpansion

REM Imposta il percorso dell'eseguibile di PlatformIO
set PLATFORMIO="C:\Users\Admin\.platformio\penv\Scripts\platformio.exe"

REM Cicla su ogni ambiente definito in platformio.ini
for /f "tokens=1 delims=[]" %%A in ('findstr /r "^\[env:.*\]" platformio.ini') do (
    set LINE=%%A
    set ENV=!LINE:env:=!
    echo 🔧 Compilazione di !ENV!...
    %PLATFORMIO% run -e !ENV!
    if errorlevel 1 (
        echo ❌ Errore durante la compilazione di !ENV!
        exit /b 1
    )
)

echo ✅ Tutte le compilazioni completate con successo.
pause
