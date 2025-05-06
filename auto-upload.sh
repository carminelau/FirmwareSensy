#!/bin/bash

# Directory del PlatformIO CLI su Windows
PLATFORMIO="C:/Users/Admin/.platformio/penv/Scripts/platformio.exe"

# Ambiente e porta
ENV="sensy_2024_V4_black"
PORT="COM9"

# Numero di dispositivi
COUNT=25

for ((i=1; i<=COUNT; i++)); do
    echo "Upload $i di $COUNT su $PORT..."
    "$PLATFORMIO" run --target upload --environment "$ENV" --upload-port "$PORT"

    if [ $? -ne 0 ]; then
        echo "❌ Upload $i fallito."
    else
        echo "✅ Upload $i riuscito."
    fi

    echo "-----------------------------------"
    read -p "Collega il prossimo dispositivo e premi INVIO per continuare..."
done
