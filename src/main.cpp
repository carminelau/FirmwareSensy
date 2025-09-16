#include "main.h"

void setup()
{

    Serial.begin(9600); // monitor Seriale
    Serial.println(F("Start Program..."));

#if LED_TYPE == 2
    Serial.println("LED RGB BLUE");
    neopixelWrite(LEDRGB_PIN, 0, 0, 255);
#endif
#if LED_TYPE == 1
    Serial.println("LED GRB BLUE");
    neopixelWrite(LEDRGB_PIN, 0, 0, 255);
#endif

    // create a task that will be executed in the loop_1_core() function, with priority 1 and executed on core 1
    xTaskCreatePinnedToCore(
        loop_1_core, /* Task function. */
        "Task2",     /* name of task. */
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        1,           /* priority of the task */
        &Task2,      /* Task handle to keep track of created task */
        1);          /* pin task to core 1 */

    EEPROM.begin(512); // inizializzo EEprom

#if defined(BUTTON_RESET_PIN)
    pinMode(BUTTON_RESET_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(BUTTON_RESET_PIN), read_reset_button, CHANGE);
#endif

    check_vergin_eeprom();

    // delete_info_sensy();
    // delete_wifi_settings();

    delay(500);
    get_mac_address();
    delay(500);
    read_topic_eeprom();
    read_version_eeprom();

    strcpy(ssidAP, "SENSY_");

    strncpy(psswdAP, myConcatenation, 9);
    strncpy(psswdAPssid, myConcatenation, 9);
    psswdAP[9] = '\0';

    strcat(ssidAP, psswdAP);

    delay(500);

    conf = read_conf_eeprom();
    wifi = read_wifi_eeprom();
    low = read_low_eeprom();

    if (!conf)
    {
#if LED_TYPE == 2
        Serial.println("LED RGB RED");
        neopixelWrite(LEDRGB_PIN, 255, 0, 0);
#endif
#if LED_TYPE == 1
        Serial.println("LED GRB RED");
        neopixelWrite(LEDRGB_PIN, 0, 255, 0);
#endif
    }
    else
    {
#if LED_TYPE == 2
        Serial.println("LED RGB GREEN");
        neopixelWrite(LEDRGB_PIN, 0, 255, 0);
#endif
#if LED_TYPE == 1
        Serial.println("LED GRB GREEN");
        neopixelWrite(LEDRGB_PIN, 255, 0, 0);
#endif
    }

    init_wifi();
    init_i2c();
    init_mqtt();
    delay(500);
    GPSsensor = init_gps();

    init_spiffs();
    delay(500);

    topicListen = topic + "GESTORE";
    configTime(0, 0, ntpServer, ntpServer2);
    set_timezone(timezone_it);

    if (low)
    {
        esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    }

    if (!low)
    {
        if (wifi)
        {
            check_update_OTA();
            configTime(0, 0, ntpServer, ntpServer2);
        }
    }

    if (sd)
    {
        String binFilePath = "";
        File updateBin = find_first_bin_file(SD, "/", binFilePath);

        if (updateBin)
        {
            // check if the name of the file is the same as nameBinESP
            if (!(String(binFilePath).indexOf(nameBinESP) != -1))
            {
                Serial.println("Update from SD card with different name");
                updateFromFile(updateBin, binFilePath.c_str());
            }
            else
            {
                Serial.println("Update from SD card with same name");
                updateFromFile(updateBin, binFilePath.c_str());
            }
        }
        else
        {
            Serial.println("Nessun file .bin trovato, nessun aggiornamento eseguito.");
        }
    }

    Serial.println("-------------------------------------");
    Serial.println("|       PARAMETER SETTINGS         |");
    Serial.println("-------------------------------------");
    Serial.print("| CONF:          ");
    Serial.println(conf);
    Serial.print("| WIFI:          ");
    Serial.println(wifi);
    Serial.print("| LOW:           ");
    Serial.println(low);
    Serial.print("| MAC:           ");
    Serial.println(myConcatenation);
    Serial.print("| EEPROM Ver.:   ");
    Serial.println(nameBinESP);
    Serial.print("| Topic:         ");
    Serial.println(topic);
    Serial.print("| TopicListen:   ");
    Serial.println(topicListen);
    Serial.println("-------------------------------------");
    Serial.println("|     ACCESS POINT SETTINGS        |");
    Serial.println("-------------------------------------");
    Serial.print("| SSID AP:       ");
    Serial.println(ssidAP);
    Serial.print("| PSSWD AP:      ");
    Serial.println(psswdAP);
    Serial.println("-------------------------------------");

    xTaskCreatePinnedToCore(
        loop_0_core, /* Task function. */
        "Task1",     /* name of task. */
        10000,       /* Stack size of task */
        NULL,        /* parameter of the task */
        2,           /* priority of the task */
        &Task1,      /* Task handle to keep track of created task */
        0);          /* pin task to core 0 */
    delay(500);
}

void loop()
{
}

void loop_0_core(void *pvParameters)
{
    for (;;)
    {
        Serial.print(F("Task1 started on core "));
        Serial.println(xPortGetCoreID());

        saveCounterSD = get_count_data_saved(SD);
        saveCounterSPIFFS = get_count_data_saved(SPIFFS);

        if ((saveCounterSD > 20 || saveCounterSPIFFS > 20) && conf)
        {
            connect_wifi_network(); // dovrebbe iniziare la connessione

            int status_wifi = WiFi.status();
            Serial.println();
            Serial.println("WiFi Status: " + String(status_wifi));

            if (status_wifi != WL_CONNECTED)
            {
                create_access_point();
                accesspoint = true;
#if LED_TYPE == 2
                Serial.println("LED RGB AQUA AP ON AFTER 20 FILES");
                neopixelWrite(LEDRGB_PIN, 0, 255, 255); // Aqua
#endif
#if LED_TYPE == 1
                Serial.println("LED GRB AQUA AP ON AFTER 20 FILES");
                neopixelWrite(LEDRGB_PIN, 255, 0, 255); // Aqua
#endif
            }
            else
            {
                accesspoint = false;
#if LED_TYPE == 2
                Serial.println("LED RGB GREEN");
                neopixelWrite(LEDRGB_PIN, 0, 255, 0); // Green
#endif
#if LED_TYPE == 1
                Serial.println("LED GRB GREEN");
                neopixelWrite(LEDRGB_PIN, 255, 0, 0); // Green
#endif
            }
        }

        if (connected)
        {
            wifi = true;
            write_inside_eeprom(wifi, 97);
            delay(500);
            if (wifi && !accesspoint)
            {
                delay(2000);
                Serial.println("Row 217: Disconnecting WiFi...");
                Serial.println("WiFi: " + String(wifi));
                Serial.println("Access Point: " + String(accesspoint));
                disconnect_access_point();
            }
        }
        else
        {
            delay(30000);
        }

        if (!conf)
        {
            if (wifi)
                check_reply_ID();
        }
        else
        {
            if (wifi && !accesspoint)
            {
                delay(5000);
                Serial.println("Row 238: Disconnecting WiFi...");
                Serial.println("WiFi: " + String(wifi));
                Serial.println("Access Point: " + String(accesspoint));
                disconnect_access_point();
            }

            if (!accesspoint)
            {
                WiFi.mode(WIFI_STA);

                if (WiFi.status() != WL_CONNECTED)
                {
                    connect_wifi_network();
                }
            }

            timeNow = millis();

            if (resetCount == 0)
            {
                check_sensors_diagnostics();
                delay(500);
                send_sensors_diagnostics();
                delay(500);
                init_rtc();
            }
            if (resetCount == 15 || resetCount == 30)
            {
                init_rtc();
            }

            if (resetCount == 5 || resetCount == 10 || resetCount == 15 || resetCount == 20 || resetCount == 25 || resetCount == 30)
            {
                if (!low)
                {
                    if (wifi)
                    {
                        check_update_OTA();
                    }
                }
                if (sd)
                {
                    String binFilePath = "";
                    File updateBin = find_first_bin_file(SD, "/", binFilePath);

                    if (updateBin)
                    {
                        // check if the name of the file is the same as nameBinESP
                        if (!(String(binFilePath).indexOf(nameBinESP) != -1))
                        {
                            Serial.println("Update from SD card with different name");
                            updateFromFile(updateBin, binFilePath.c_str());
                        }
                        else
                        {
                            Serial.println("Update from SD card with same name");
                            updateFromFile(updateBin, binFilePath.c_str());
                        }
                    }
                    else
                    {
                        Serial.println("Nessun file .bin trovato, nessun aggiornamento eseguito.");
                    }
                }
            }

            if (pmsa003)
            {
                delay(4500);
            }

            if (ozone)
            {
                O3 = read_ozone();
                delay(250);
            }

            if (scd30)
            {
                read_scd30();
                delay(250);
            }

            if (scd41)
            {
                read_scd4x();
                delay(250);
            }

            if (gas)
            {
                read_multigas();
                delay(250);
            }
            if (sps)
            {
                sps30_start_measurement();
                delay(500);
                if (!read_sps30(&pmAe1_0, &pmAe2_5, &pmAe10_0))
                { // se non funziona l'sps

                    if (sen55)
                    {
                        if (!read_sen55())
                        { // se non funziona il sensore di polveri SEN55
                            if (pmsa003)
                            {
                                read_pmsA003(); // campiono dal pms
                            }
                        }
                    }
                    else
                    {
                        if (pmsa003)
                        {
                            read_pmsA003(); // campiono dal pms
                        }
                    }
                }
            }
            else
            {
                if (sen55)
                {
                    if (!read_sen55())
                    { // se non funziona il sensore di polveri SEN55
                        if (pmsa003)
                        {
                            read_pmsA003(); // campiono dal pms
                        }
                    }
                }
                else
                {
                    if (pmsa003)
                    {
                        read_pmsA003(); // campiono dal pms
                    }
                }
            }

            if (mics4514)
            {
                co = read_mics(CO, "CO");
                no2 = read_mics(NO2, "NO2");
                nh3 = read_mics(NH3, "NH3");
            }

            if (sps)
            {
                print_sps30_values(pmAe1_0, pmAe2_5, pmAe10_0);
            }
            if (ane)
            {
                read_anemometer();
            }
            if (soil)
            {
                read_soil_moisture();
            }

            epochs = get_epoch();
            if (epochs == 0)
            {
                epochs = get_epoch_ntp_server();
                Serial.println("Epochs NTP: " + String(epochs));

                if (epochs == 0)
                {
                    epochs = get_epoch_storage();
                    Serial.println("Epochs File: " + String(epochs));
                }
            }

            doc["ID"] = topic; // aggiungo i campi
            doc["stato_GPS"] = GPSsensor;
            doc["timestamp"] = epochs;

            if (GPSsensor && latitude != 0 && longitude != 0)
            {
                doc["lat"] = latitude;
                doc["lon"] = longitude;
                doc["siv"] = SIV;
            }

            if (lux)
            {
                float lux = lightMeter.readLightLevel();
                if (lux > 0)
                {
                    doc["luminosita"] = lux;
                }
            }

            if (mics4514)
            {
                if (no2 > 0 && no2 < 1000)
                {
                    doc["no2"] = no2;
                }
                if (nh3 > 0 && nh3 < 1000)
                {
                    doc["nh3"] = nh3;
                }
                if (co > 0 && co < 2000)
                {
                    doc["co"] = co;
                }
            }
            if (sps || pmsa003 || sen55)
            {
                if (pmAe2_5 > 0)
                {
                    doc["pm2_5"] = round_float(pmAe2_5);
                }
                if (pmAe10_0 > 0)
                {
                    doc["pm10"] = round_float(pmAe10_0);
                }
                if (pmAe1_0 > 0)
                {
                    doc["pm1"] = round_float(pmAe1_0);
                }
            }
            if (sht)
            {
                float temp = sht21.getTemperature();
                float hum = sht21.getHumidity();
                if (temp > -20 && temp < 100)
                {
                    doc["temperatura"] = temp;
                }
                if (hum > 10 && hum < 101)
                {
                    doc["umidita"] = hum;
                }
            }

            if (scd30)
            {
                if (scd30_temp > -20 && scd30_temp < 100)
                {
                    doc["temperatura"] = scd30_temp;
                }
                if (scd30_hum > 10 && scd30_hum < 101)
                {
                    doc["umidita"] = scd30_hum;
                }
                if (scd30_co2 > 0 && scd30_co2 < 10000)
                {
                    doc["co2"] = scd30_co2;
                }
            }

            if (scd41)
            {
                if (scd41_temp > -20 && scd41_temp < 100)
                {
                    doc["temperatura"] = scd41_temp;
                }
                if (scd41_hum > 10 && scd41_hum < 101)
                {
                    doc["umidita"] = scd41_hum;
                }
                if (scd41_co2 > 0 && scd41_co2 < 10000)
                {
                    doc["co2"] = scd41_co2;
                }
            }

            if (gas)
            {
                if (no2 > 0 && no2 < 1000)
                {
                    doc["no2"] = no2;
                }
                if (voc > 0 && voc < 1000)
                {
                    doc["voc"] = voc;
                }
                if (co > 0 && co < 2000)
                {
                    doc["co"] = co;
                }
                if (c2h5oh > 0 && c2h5oh < 1000)
                {
                    doc["c2h5oh"] = c2h5oh;
                }
            }

            if (sen55)
            {
                if (sen55_temp > -20 && sen55_temp < 100)
                {
                    doc["temperatura"] = sen55_temp;
                }
                if (sen55_hum > 10 && sen55_hum < 101)
                {
                    doc["umidita"] = sen55_hum;
                }
                if (voc > 0 && voc < 1000)
                {
                    doc["voc"] = voc;
                }
                if (no2_index > 0 && no2_index < 1000)
                {
                    doc["no2"] = no2_index;
                }
            }

            if (ane)
            {
                if (temperature_ane > -20 && temperature_ane < 100)
                {
                    doc["temperatura"] = temperature_ane;
                }
                if (humidity_ane > 0 && humidity_ane < 101)
                {
                    doc["umidita"] = humidity_ane;
                }
                if (pressure_ane > 800 && pressure_ane < 1200) 
                {
                    doc["pressione"] = pressure_ane;
                }

                doc["direzione_vento"] = dir_wind_fix(windDirection_ane);
                doc["intensita_vento"] = windSpeed_ane;
            }
            if (ozone)
            {
                if (O3 > 0 && O3 < 10000)
                {
                    doc["o3"] = O3;
                }
            }
            if (soil)
            {
                if (soil_ph > 0 && soil_ph < 14)
                {
                    doc["soil_ph"] = soil_ph;
                }
                if (soil_temperature > -3 && soil_temperature < 100)
                {
                    doc["soil_temp"] = soil_temperature;
                }
                if (soil_humidity > 0 && soil_humidity < 101)
                {
                    doc["soil_hum"] = soil_humidity;
                }
                if (soil_conductivity > 0 && soil_conductivity < 1000)
                {
                    doc["soil_cond"] = soil_conductivity;
                }
                if (soil_nitrogen > 0 && soil_nitrogen < 1000)
                {
                    doc["soil_nitrogen"] = soil_nitrogen;
                }
                if (soil_phosphorus > 0 && soil_phosphorus < 1000)
                {
                    doc["soil_phosphorus"] = soil_phosphorus;
                }
                if (soil_potassium > 0 && soil_potassium < 1000)
                {
                    doc["soil_potassium"] = soil_potassium;
                }
            }

            // check if all value of Pollutants is in keys od doc
            for (String pollutant : Pollutants)
            {
                if (!doc.containsKey(pollutant))
                {
                    PollutantsMissing.push_back(pollutant);
                }
            }

            serializeJson(doc, jsonOutput); // ottengo la stringa da inviare campionata in questo ciclo

            Serial.println(jsonOutput);

            String pollutantMissing = vector_to_encoded_json_array(PollutantsMissing);
            Serial.println("Pollutants Missing: " + pollutantMissing);
            if (pollutantMissing != "[]")
            {
                get_nearest_data(pollutantMissing);
                PollutantsMissing.clear();
            }

            serializeJson(doc, jsonOutput); // ottengo la stringa da inviare campionata in questo ciclo

            Serial.println(jsonOutput);

            if (WiFi.status() != WL_CONNECTED && !accesspoint)
            {
                connect_wifi_network();
            }

            if (wifi)
            { // se c'è wifi mando col wifi

                if (!send_data_mqtt())
                { // se non va il wifi

                    Serial.println(F("NO WIfI Local save mode"));

                    serializeJson(doc, jsonOutput);
                    write_file_data(jsonOutput);
                }
                // azzero il numero di json e svuoto il buffer contentente le stringhe json
                doc.clear();
                info.clear();
                checkSensor.clear();
                resetCount++;

                if (low)
                {
                    setCpuFrequencyMhz(80);
                }
                Serial.print("LISTEN MQTT ");
                Serial.println(millis());
                for (int i = 0; i < 2000; i++)
                {
                    loop_mqtt();
                }
                Serial.print("FINISH LISTEN MQTT ");
                Serial.println(millis());
                diffe = millis() - timeNow + 35000;
                Serial.print("Execution Time: ");
                Serial.println(diffe);
                set_rtc(epochs + diffe / 1000);
                delete_message_received_mqtt();

                Serial.flush();

                if (resetCount == 50)
                {
                    ESP.restart();
                }
                if (low)
                {
                    WiFi.disconnect();
                    if (sen55)
                    {
                        sen5x.setFanAutoCleaningInterval(0); // disabilito la pulizia automatica della ventola
                        sen5x.stopMeasurement(); // stop del sensore SEN55
                    }
                    setCpuFrequencyMhz(10);

                    esp_deep_sleep_start();
                }
            }
        }
    }
}

void loop_1_core(void *pvParameters)
{
    Serial.print("Task2 running on core ");
    Serial.println(xPortGetCoreID());
    int num_it = 0;
    for (;;)
    {
        if (GPSsensor)
        {

            latitude = (double)myGNSS.getLatitude() / 10000000;

            longitude = (double)myGNSS.getLongitude() / 10000000;

            SIV = myGNSS.getSIV();
            // If 5000 milliseconds pass and there are no characters coming in
            // over the software Serial port, show a "No GPS detected" error
            if (millis() > 5000 && (!myGNSS.isConnected()))
            {
                Serial.println("No GPS detected");
                delay(1000);
            }
        }
        check_pressing_button();
        if (daresettare)
        {
            press_long_time_button();
            ESP.restart();
        }
    }
}

int dir_wind_fix(int dire)
{
    int dir = 0;
    if (dire == 0)
    {
        dir = 0;
    }
    else if ((dire > 0) and (dire < 22))
    {
        if (dire < (22 - dire))
        {
            dir = 0;
        }
        else
        {
            dir = 1;
        }
    }
    else if (dire == 22)
    {
        dir = 1;
    }
    else if ((dire > 22) and (dire < 45))
    {
        if ((dire - 22) < (45 - dire))
        {
            dir = 1;
        }
        else
        {
            dir = 2;
        }
    }
    else if (dire == 45)
    {
        dir = 2;
    }
    else if ((dire > 45) and (dire < 67))
    {
        if ((dire - 45) < (67 - dire))
        {
            dir = 2;
        }
        else
        {
            dir = 3;
        }
    }
    else if (dire == 67)
    {
        dir = 3;
    }
    else if ((dire > 67) and (dire < 90))
    {
        if ((dire - 67) < (90 - dire))
        {
            dir = 3;
        }
        else
        {
            dir = 4;
        }
    }
    else if (dire == 90)
    {
        dir = 4;
    }
    else if ((dire > 90) and (dire < 112))
    {
        if ((dire - 90) < (112 - dire))
        {
            dir = 4;
        }
        else
        {
            dir = 5;
        }
    }
    else if (dire == 112)
    {
        dir = 5;
    }
    else if ((dire > 112) and (dire < 135))
    {
        if ((dire - 112) < (135 - dire))
        {
            dir = 5;
        }
        else
        {
            dir = 6;
        }
    }
    else if (dire == 135)
    {
        dir = 6;
    }
    else if ((dire > 135) and (dire < 157))
    {
        if ((dire - 135) < (157 - dire))
        {
            dir = 6;
        }
        else
        {
            dir = 7;
        }
    }
    else if (dire == 157)
    {
        dir = 7;
    }
    else if ((dire > 157) and (dire < 180))
    {
        if ((dire - 157) < (180 - dire))
        {
            dir = 7;
        }
        else
        {
            dir = 8;
        }
    }
    else if (dire == 180)
    {
        dir = 8;
    }
    else if ((dire > 180) and (dire < 202))
    {
        if ((dire - 180) < (202 - dire))
        {
            dir = 8;
        }
        else
        {
            dir = 9;
        }
    }
    else if (dire == 202)
    {
        dir = 9;
    }
    else if ((dire > 202) and (dire < 225))
    {
        if ((dire - 202) < (225 - dire))
        {
            dir = 9;
        }
        else
        {
            dir = 10;
        }
    }
    else if (dire == 225)
    {
        dir = 10;
    }
    else if ((dire > 225) and (dire < 247))
    {
        if ((dire - 225) < (247 - dire))
        {
            dir = 10;
        }
        else
        {
            dir = 11;
        }
    }
    else if (dire == 247)
    {
        dir = 11;
    }
    else if ((dire > 247) and (dire < 270))
    {
        if ((dire - 247) < (270 - dire))
        {
            dir = 11;
        }
        else
        {
            dir = 12;
        }
    }
    else if (dire == 270)
    {
        dir = 12;
    }
    else if ((dire > 270) and (dire < 292))
    {
        if ((dire - 270) < (292 - dire))
        {
            dir = 12;
        }
        else
        {
            dir = 13;
        }
    }
    else if (dire == 292)
    {
        dir = 13;
    }
    else if ((dire > 292) and (dire < 315))
    {
        if ((dire - 292) < (315 - dire))
        {
            dir = 13;
        }
        else
        {
            dir = 14;
        }
    }
    else if (dire == 315)
    {
        dir = 14;
    }
    else if ((dire > 315) and (dire < 337))
    {
        if ((dire - 315) < (337 - dire))
        {
            dir = 14;
        }
        else
        {
            dir = 15;
        }
    }
    else if (dire == 337)
    {
        dir = 15;
    }
    else if ((dire > 337) and (dire < 359))
    {
        if ((dire - 337) < (359 - dire))
        {
            dir = 15;
        }
        else
        {
            dir = 0;
        }
    }
    return dir;
}

float round_float(float value)
{
    return (int)(value * 100 + 0.5) / 100.0;
}

bool init_relay(int relayPin)
{
    pinMode(relayPin, OUTPUT);    // Inizializzo il relay
    digitalWrite(relayPin, HIGH); // Provo ad attivarlo
    delay(10);                    // Piccola attesa per stabilizzazione

    if (digitalRead(relayPin) != HIGH)
    {
        return false; // Se il valore letto non corrisponde, il relay potrebbe non essere presente
    }

    digitalWrite(relayPin, LOW); // Disattivo il relay per sicurezza
    return true;                 // Il relay sembra essere presente
}

void create_access_point()
{
    WiFi.disconnect();
    Serial.println("Setting AP (Access Point)…");

    WiFi.softAP(ssidAP, psswdAPssid); // inizializzo l'access point con ssid e password e imposto il numero di connessioni massime
    IPAddress IP = WiFi.softAPIP();   // indirizzo AccessPoint
    Serial.println();

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", "ok"); });

    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
              daresettare = true;
              request->send(200, "text/plain", "ok"); });

    server.on("/isGPS", HTTP_GET, [](AsyncWebServerRequest *request)
              { if(GPSsensor)
                request->send(200, "text/plain", "true");
              else
                request->send(200, "text/plain", "false"); });

    server.on("/getMacAddress", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", myConcatenation); });

    server.on("/scanWifi", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                jsonWifi = get_list_wifi();
                request->send(200, "text/json", jsonWifi); });

    server.on("/configWifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("ssid",true))
    {                               // se la request ha i parametri ssid
      char input_ssid[32] = "";//Stringa (puntatore a char) in cui salvo ssid immesso da form
      char input_psswd[64] = "";//Stringa (puntatore a char) in cui salvo password immesso da form

      String eeprom_ssid = request->getParam("ssid",true)->value();//Stringa in cui salvo ssid immesso da form
      String eeprom_psswd = request->getParam("pwd",true)->value();//Stringa in cui salvo password immesso da form

      eeprom_ssid.toCharArray(input_ssid,32);
      eeprom_psswd.toCharArray(input_psswd,64);

      EEPROM.writeString(0,input_ssid);
      EEPROM.writeString(32,input_psswd);

      Serial.println(input_ssid);
      Serial.println(input_psswd);

      EEPROM.commit();
      delay(500);

      if (String(input_ssid)!="") {//se password e ssid sono stati inseriti

        if (String(input_ssid) == "") {
          WiFi.begin(input_ssid);
        } else {
          WiFi.begin(input_ssid, input_psswd);//accedo al wifi con le credenziali inserite da form
        }
        
        delay(250);
        connected=false;
        for (int i = 0; i < 5; i++)
        {
          Serial.print(".");
          if (WiFi.status() == WL_CONNECTED)
          {
            break;
          }
          delay(800);
        }
        Serial.println("");
      if (WiFi.status() == WL_CONNECTED) {//se la connessione è fallita o non esiste l'ssid
        connected=true;
        request->send(200, "text/plain", "connected");
      } else {
      request->send(200, "text/plain", "error");
        }
      }
      else {
        request->send(200, "text/plain", "errorNOSSID");
      }
    } });
    server.begin(); // inizializzo il server
}

void init_wifi()
{ // inizializzazione wifi
    WiFi.persistent(false);
    WiFi.disconnect();
    if (conf)
    {
        WiFi.mode(WIFI_STA);
        if (WiFi.status() != WL_CONNECTED)
        {
            connect_wifi_network();
        }
    }
    else
    {
        WiFi.mode(WIFI_AP);
        create_access_point();
    }
}

void init_i2c()
{
    Wire.begin(SDA_PIN, SCL_PIN); // Wire communication begin
    Serial.begin(9600);           // The baudrate of Serial monitor is set in 9600
    while (!Serial)
        ; // Waiting for Serial Monitor
}

bool init_sps30()
{
    int16_t ret;
    uint8_t auto_clean_days = 4;
    bool begin;

    sensirion_i2c_init();

    if (sps30_probe() != 0)
    {
        Serial.println("SPS30 FAILED");
        return false;
    }
    else
    {
        Serial.println("SPS30 DONE");
        begin = true;
    }

    ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
    if (ret)
    {
        Serial.println("error setting the auto-clean interval");
        begin = false;
    }

    ret = sps30_start_measurement();
    if (ret < 0)
    {
        Serial.println("error starting measurement\n");
        begin = false;
    }

    if (!read_sps30(&pmAe1_0, &pmAe2_5, &pmAe10_0))
    {
        begin = false;
    }
    else
    {
        begin = true;
    }

    return begin;
}

bool init_ozone()
{
    Ozone.setModes(MEASURE_MODE_AUTOMATIC);
    return Ozone.begin(OZONE_ADDRESS_3);
}

bool init_multigas()
{
    byte error; // variable for error and I2C address
    Wire.beginTransmission(0x08);
    error = Wire.endTransmission();

    if (error == 0)
    {
        sensore.begin(Wire, 0x08); // use the hardware I2C
        return true;
    }
    else
    {
        return false;
    }
}

bool sht21_init()
{
    byte error; // variable for error and I2C address
    Wire.beginTransmission(0x40);
    error = Wire.endTransmission();

    if (error == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool check_anemometer()
{
    Serial.println("Initializing HYWDC6E sensor...");

    AnemometerData anemData = sensors.readAnemometer(9600);
    sensors.printAnemometerData(anemData);

    if (anemData.valid)
    {
        Serial.println("Anemometer initialized successfully.");
        return true;
    }
    else
    {
        Serial.println("Failed to initialize Anemometer.");
        return false;
    }
}

void init_spiffs()
{
    if (SPIFFS.begin())
    {

        Serial.println("INIT SPIFFS SUCCESSFUL");
    }
    else
    {
        if (SPIFFS.begin(true))
        {

            Serial.println("INIT SPIFFS SUCCESSFUL WITH FORMAT");
        }

        else
        {

            Serial.println("INIT SPIFFS FAILED");
        }
    }
}

void init_rtc()
{
    int timestamp = get_epoch();
    if (timestamp > 0)
    {
        char numFile[12];
        itoa(timestamp, numFile, 10);
        Serial.println("SPIFFS");
        write_file_storage(SPIFFS, "/e.txt", numFile);

        if (sd)
        {
            Serial.println("SD");
            write_file_storage(SD, "/e.txt", numFile);
        }
    }
    else
    {
        timestamp = get_epoch_ntp_server();
        if (timestamp > 0)
        {
            char numFile[12];
            itoa(timestamp, numFile, 10);
            Serial.println("SPIFFS");
            write_file_storage(SPIFFS, "/e.txt", numFile);

            if (sd)
            {
                Serial.println("SD");
                write_file_storage(SD, "/e.txt", numFile);
            }
        }
        else
        {
            Serial.println("read from SPIFFS");
            String number = read_file_storage(SPIFFS, "/e.txt");
            if (number == "")
            {
                Serial.println("read from SD");
                number = read_file_storage(SD, "/e.txt");
            }

            Serial.println(number);
        }
    }
}

bool init_pmsA003(void)
{
    int somma = 0;

    delay(10000);
    pms.read();
    somma = pms.pm01 + pms.pm25 + pms.pm10;
    Serial.print("Sum of PM: ");
    Serial.println(somma);
    for (int i = 0; i < 3; i++)
    {
        if (somma > 0)
        {
            return true;
        }
        else
        {
            delay(10000);
            pms.read();
            somma = pms.pm01 + pms.pm25 + pms.pm10;
        }
    }
    return false;
}

int get_epoch()
{
    String response = "";
    const char resource[] = "/epoch";

    WiFiClient clientWifi;
    HttpClient httpWifi(clientWifi, host, port);

    int err = httpWifi.get(resource);
    if (err != 0)
    {

        Serial.println(F("ERROR"));
        return 0;
    }
    else
    {
        int status = httpWifi.responseStatusCode();

        Serial.print(F("Response status code: "));
        Serial.println(status);
        if (status == 200)
        {
            response = httpWifi.responseBody();

            Serial.println(F("Response:"));
            Serial.println(response);
        }
        else
        {

            Serial.println(F("errore config data"));
            return 0;
        }
    }

    return response.toInt();
}

void set_rtc(int timestamp)
{

    if (timestamp > 0)
    {
        char numFile[12];
        itoa(timestamp, numFile, 10);
        Serial.println("SPIFFS");
        write_file_storage(SPIFFS, "/e.txt", numFile);
        if (sd)
        {
            Serial.println("SD");
            write_file_storage(SD, "/e.txt", numFile);
        }
    }
    else
    {
        Serial.println("read from SPIFFS");
        String number = read_file_storage(SPIFFS, "/e.txt");
        if (number == "")
        {
            Serial.println("read from SD");
            number = read_file_storage(SD, "/e.txt");
        }

        Serial.println(number);
    }
}

String read_file_storage(fs::FS &fs, const char *path)
{

    Serial.printf("Reading file: %s\r\n", path);
    File file = fs.open(path);
    if (!file || file.isDirectory())
    {

        Serial.println("- failed to open file for reading");
    }

    String stringa = "";
    while (file.available())
    {
        stringa = file.readString();

        Serial.println(stringa);
    }
    file.close();
    return stringa;
}

bool write_file_storage(fs::FS &fs, const char *path, const char *message)
{
    delay(1000);
    Serial.printf("Writing file: %s\r\n", path);
    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("- failed to open file for writing");
        return false;
    }
    if (file.print(message))
    {

        Serial.println(message);
        return true;
    }
    else
    {
        Serial.println("- write failed");
        return false;
    }
    file.close();
}

void check_sensors_diagnostics()
{

    // clear the json object
    checkSensor.clear();
    info.clear();
    sensors.begin();
    sensors.enableDebug(true); // Abilita output debug

#if RELAY == 1
    relay1 = init_relay(RELAY1_PIN);
    relay2 = init_relay(RELAY2_PIN);
#else
    relay1 = false;
    relay2 = false;
#endif

    Serial.println("START INITIALIZATION SENSORS");

    pms.init();
    sps = init_sps30();
    ozone = init_ozone();
    gas = init_multigas();
    sht = sht21_init();
    GPSsensor = init_gps();
    pmsa003 = init_pmsA003();
    sen55 = init_sen55();
    scd30 = init_scd30();
    scd41 = init_scd4x();
    sd = init_sd_card();
    mics4514 = init_mics();
    ane = check_anemometer();
    delay(1000);
    soil = check_soil_moisture();
    // soil = true;
    lux = init_luxometer();

    saveCounterSD = get_count_data_saved(SD);
    saveCounterSPIFFS = get_count_data_saved(SPIFFS);

    Serial.println("-------------------- SENSOR CHECK DONE -----------------------------");

    checkSensor["sps"] = sps;
    checkSensor["ozone"] = ozone;
    checkSensor["gas"] = gas;
    checkSensor["pmsa003"] = pmsa003;
    checkSensor["sht"] = sht;
    checkSensor["ane"] = ane;
    checkSensor["sen55"] = sen55;
    checkSensor["scd30"] = scd30;
    checkSensor["scd41"] = scd41;
    checkSensor["gps"] = GPSsensor;
    checkSensor["mics4514"] = mics4514;
    checkSensor["luxometer"] = lux;
    checkSensor["soil_moisture"] = soil;
    checkSensor["SD"] = sd;
    if (sd)
    {
        info["FilesinSD"] = saveCounterSD;
    }
    info["FilesinSPIFFS"] = saveCounterSPIFFS;
    info["Relay1"] = relay1;
    info["Relay2"] = relay2;

    serializeJson(checkSensor, stringCheckSensor);
    serializeJson(info, stringInfo);

    Serial.println(stringCheckSensor);
    Serial.println(stringInfo);

    // per ogni variabile a true inserire una stringa nell'oggetto String *stringArray = new String[COLLECT_NUMBER];

    if (sps)
    {
        Pollutants.push_back("pm1");
        Pollutants.push_back("pm2_5");
        Pollutants.push_back("pm10");
    }
    if (pmsa003)
    {
        Pollutants.push_back("pm1");
        Pollutants.push_back("pm2_5");
        Pollutants.push_back("pm10");
    }
    if (sen55)
    {
        Pollutants.push_back("pm1");
        Pollutants.push_back("pm2_5");
        Pollutants.push_back("pm10");
        Pollutants.push_back("voc");
        Pollutants.push_back("no2");
        Pollutants.push_back("temperatura");
        Pollutants.push_back("umidita");
    }
    if (scd30)
    {
        Pollutants.push_back("co2");
        Pollutants.push_back("temperatura");
        Pollutants.push_back("umidita");
    }
    if (scd41)
    {
        Pollutants.push_back("co2");
        Pollutants.push_back("temperatura");
        Pollutants.push_back("umidita");
    }
    if (lux)
    {
        Pollutants.push_back("luminosita");
    }
    if (mics4514)
    {
        Pollutants.push_back("no2");
        Pollutants.push_back("nh3");
        Pollutants.push_back("co");
    }
    if (gas)
    {
        Pollutants.push_back("voc");
        Pollutants.push_back("no2");
        Pollutants.push_back("co");
        Pollutants.push_back("c2h5oh");
    }
    if (ozone)
    {
        Pollutants.push_back("o3");
    }
    if (ane)
    {
        Pollutants.push_back("pressione");
        Pollutants.push_back("temperatura");
        Pollutants.push_back("umidita");
        Pollutants.push_back("direzione_vento");
        Pollutants.push_back("intensita_vento");
    }
    if (sht)
    {
        Pollutants.push_back("temperatura");
        Pollutants.push_back("umidita");
    }
}

bool connect_wifi_network()
{

    Serial.println(F("Reading EEPROM ssid"));

    String esid = EEPROM.readString(0); // stringa in cui viene salvata l'ssid presente nell'eeprom
    /*RECUPERO SSID DA EEPROM*/
    if (esid.length() == 0)
    {
        Serial.println(F("SSID NOT FOUND"));
    }

    Serial.print(F("SSID: "));
    Serial.println(esid);

    Serial.println(F("Reading EEPROM pass"));

    String epass = EEPROM.readString(32); // stringa in cui viene salvata la password presente nell'eeprom
    /*RECUPERO PSSWD DA EEPROM*/

    Serial.print(F("PASS: "));
    Serial.println(epass);

    /*CONNETTO AL WIFI*/
    if (epass.length() > 0)
    { // wifi con password
        WiFi.begin((char *)esid.c_str(), (char *)epass.c_str());
        delay(1500);
    }
    else
    { // wifi senza password
        WiFi.begin((char *)esid.c_str());
        delay(1500);
    }

    for (int i = 0; i < 6; i++)
    {
        Serial.print(".");
        if (WiFi.status() == WL_CONNECTED)
        {
            break;
        }
        delay(250);
    }
    Serial.println("");

    if (WiFi.status() == WL_CONNECTED)
    {
        wifi = true;
        write_inside_eeprom(wifi, 97);

        return true;
    }
    return false;
}

void write_inside_eeprom(bool val, int addr)
{
    EEPROM.writeBool(addr, val);
    EEPROM.commit();
    delay(500);
}

void delete_wifi_settings()
{
    Serial.println("RESETTING WIFI SSID AND PWD");

    char ssid[32] = "";
    char pwd[64] = "";

    EEPROM.writeString(0, ssid);
    EEPROM.writeString(32, pwd);

    EEPROM.commit();
    delay(500);

    WiFi.disconnect(); // disconnetto wifi

    wifi = false;
    write_inside_eeprom(wifi, 97);

    ESP.restart();
}

void IRAM_ATTR disconnect_access_point()
{
    Serial.println("Disconnecting AP...");
    server.end();                 // Stop the server
    WiFi.softAPdisconnect(false); // Disconnect the AP without stopping the WiFi
    delay(1000);
}

String get_id_square()
{
    String response = "";
    const String resource = "/get_ID?mac=" + String(myConcatenation) + "&board=" + verionBoard;

    WiFiClient clientWifi;
    HttpClient httpWifi(clientWifi, host, port);

    int err = httpWifi.get(resource);
    if (err != 0)
    {

        Serial.println(F("ERROR"));

        return "Wait";
    }
    else
    {
        int status = httpWifi.responseStatusCode();

        Serial.print(F("Response status code: "));
        Serial.println(status);

        if (status == 200)
        {
            response = httpWifi.responseBody();

            Serial.print(F("Response from server: "));
            Serial.println(response);
        }
        else
        {

            Serial.println(F("errore config data"));

            return "Wait";
        }
    }

    return response;
}

void get_mac_address()
{
    char ssid1[12];
    char ssid2[12];

    uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
    uint16_t chip = (uint16_t)(chipid >> 32);

    snprintf(ssid1, 12, "%04X", chip);
    snprintf(ssid2, 12, "%08X", (uint32_t)chipid);

    sprintf(myConcatenation, "%s%s", ssid1, ssid2);
}

void write_string_eeprom(char *c, int offset)
{

    EEPROM.writeString(offset, c);
    EEPROM.commit();
    delay(1000);
    Serial.println(c);
}

void write_string_eeprom(String c, int offset)
{

    EEPROM.writeString(offset, c);
    EEPROM.commit();
    delay(1000);
    Serial.println(c);
}

void read_topic_eeprom()
{
    topic = EEPROM.readString(126);

    Serial.print("EEPROM Topic: ");
    Serial.println(topic);
}

void read_version_eeprom()
{

    nameBinESP = EEPROM.readString(104);

    Serial.print("EEPROM Version: ");
    Serial.println(nameBinESP);
}

void check_reply_ID()
{
    String id = get_id_square();
    delay(12000);
    if (WiFi.status() != WL_CONNECTED)
    {
        connect_wifi_network();
    }
#if LED_TYPE == 2
    Serial.println("LED RGB PURPLE WHEN WAITING FOR ID");
    neopixelWrite(LEDRGB_PIN, 255, 0, 255); // Purple when waiting for ID
#endif
#if LED_TYPE == 1
    Serial.println("LED GRB PURPLE WHEN WAITING FOR ID");
    neopixelWrite(LEDRGB_PIN, 0, 255, 255); // Purple when waiting for ID
#endif
    int countwait = 0;
    while (id == "Wait")
    {
        id = get_id_square();
        countwait++;
        if (countwait == 20)
        {
            press_long_time_button();
            ESP.restart();
        }
        delay(12000);
    }
    Serial.print("ID: ");
    Serial.println(id.substring(0, 14));

    Serial.print("Version: ");
    Serial.println(id.substring(15, 36));

    nameBinESP = id.substring(15, 36);
    topic = id.substring(0, 14);

    write_string_eeprom(nameBinESP, 104);
    write_string_eeprom(topic, 126);

    conf = true;

#if LED_TYPE == 2
    Serial.println("LED RGB GREEN AFTER CONFIGURATION");
    neopixelWrite(LEDRGB_PIN, 0, 255, 0);
#endif
#if LED_TYPE == 1
    Serial.println("LED GRB GREEN AFTER CONFIGURATION");
    neopixelWrite(LEDRGB_PIN, 255, 0, 0);
#endif
    write_conf_eeprom(conf);
    delay(1000);
}

void write_conf_eeprom(bool val)
{
    EEPROM.writeBool(101, val);
    EEPROM.commit();
    delay(1000);
}

void write_low_eeprom(bool val)
{
    EEPROM.writeBool(205, val);
    EEPROM.commit();
    delay(1000);
}

bool read_conf_eeprom()
{
    return EEPROM.readBool(101);
}

bool read_wifi_eeprom()
{
    return EEPROM.readBool(97);
}

bool read_low_eeprom()
{
    return EEPROM.readBool(205);
}

void delete_info_sensy()
{
    char identificativo[14] = "";
    char version[22] = "";

    EEPROM.writeString(104, version);
    EEPROM.writeString(126, identificativo);

    write_low_eeprom(false);
    write_conf_eeprom(false);
    EEPROM.commit();
    delay(500);
}

void send_sensors_diagnostics()
{
    String response = "";
    const String resource = "/set_sensors?sensors=" + String(stringCheckSensor) + "&ID=" + topic + "&versione=" + nameBinESP + "&info=" + String(stringInfo);
    WiFiClient clientWifi;
    HttpClient httpWifi(clientWifi, host, port);

    int err = httpWifi.get(resource);
    if (err != 0)
    {

        Serial.println(F("ERROR"));
    }
    else
    {
        int status = httpWifi.responseStatusCode();

        Serial.print(F("Response status code: "));
        Serial.println(status);

        if (status == 200)
        {
            response = httpWifi.responseBody();

            Serial.println(F("Response:"));
            Serial.println(response);
        }
        else
        {

            Serial.println(F("errore config data"));
        }
    }
}

void check_update_OTA()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        connect_wifi_network();
    }

    read_version_eeprom();

    delay(2000);

    String response = "";
    const String resource = "/versione_firmware?ID=" + topic;

    WiFiClient clientWifi;
    HttpClient httpWifi(clientWifi, host, port);

    int err = httpWifi.get(resource);
    if (err != 0)
    {

        Serial.println(F("ERROR"));
    }
    else
    {
        int status = httpWifi.responseStatusCode();

        Serial.print(F("Response status code: "));
        Serial.println(status);

        if (status == 200)
        {
            response = httpWifi.responseBody();

            Serial.println(F("Response:"));
            Serial.println(response);
        }
        else
        {

            Serial.println(F("errore config data"));
        }
    }
    unsigned long timeout = millis();

    if (millis() - timeout > 5000)
    {

        Serial.println("clientota Timeout !");

        clientota.stop();
        return;
    }

    nameBinServer = response;
    nameBinServer.trim();
    if (!nameBinServer.length())
    {

        Serial.println(F("No reply from server for this name of version"));
    }

    Serial.println("versione server " + nameBinServer);
    Serial.println("versione ESP " + nameBinESP);

    if (!nameBinServer.isEmpty())
    {
        if (!nameBinServer.equals(nameBinESP))
        {
            exec_update_OTA();
        }
        else
        {

            Serial.println("Sensy already updated");

            return;
        }
    }
    else
    {

        Serial.println("Connection Error");
    }
}

void exec_update_OTA()
{

    Serial.println("Connecting to: SenseSquare for OTA");

    if (clientota.connect(host.c_str(), port))
    {

        // Get the contents of the bin file
        clientota.print(String("GET /aggiornamento_firmware?versione=") + nameBinServer + " HTTP/1.1\r\n" +
                        "Host: " + host + "\r\n" +
                        "Cache-Control: no-cache\r\n" +
                        "Connection: close\r\n\r\n");

        unsigned long timeout = millis();
        while (clientota.available() == 0)
        {
            if (millis() - timeout > 5000)
            {

                Serial.println("clientota Timeout !");

                clientota.stop();
                return;
            }
        }
        // Once the response is available,
        // check stuff

        while (clientota.available())
        { //   read line till /n
            String line = clientota.readStringUntil('\n');
            // remove space, to check if the line is end of headers
            line.trim();

            // if the the line is empty,
            // this is end of headers
            // break the while and feed the
            // remaining `clientota` to the
            // Update.writeStream();
            if (!line.length())
            {
                // headers ended
                break; // and get the OTA started
            }

            // Check if the HTTP Response is 200
            // else break and Exit Update
            if (line.startsWith("HTTP/1.1"))
            {
                if (line.indexOf("200") < 0)
                {

                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");

                    break;
                }
            }

            // extract headers here
            // Start with content length
            if (line.startsWith("Content-Length: "))
            {
                contentLength = atol((get_header_value(line, "Content-Length: ")).c_str());

                Serial.println("Got " + String(contentLength) + " bytes from server");
            }
        }
    }
    else
    {
        // May be try?
        // Probably a choppy network?

        Serial.println("Connection to " + String(host) + " failed. Please check your setup");

        // retry??
        // exec_update_OTA();
    }

    // Check what is the contentLength and if content type is `application/octet-stream`

    Serial.println("contentLength : " + String(contentLength));

    // check contentLength and content type
    if (contentLength > 0)
    {
        // Check if there is enough to OTA Update
        bool canBegin = Update.begin(contentLength);

        // If yes, begin
        if (canBegin)
        {

            Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");

            // No activity would appear on the Serial monitor
            // So be patient. This may take 2 - 5mins to complete

            Serial.println(clientota.available());

            size_t written = Update.writeStream(clientota);
            // size_t written =Update.write((uint8_t*)datas.c_str(),int(contentLength));
            if (written == contentLength)
            {

                Serial.println("Written : " + String(written) + " successfully");
            }
            else
            {

                Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");

                // retry??
                // exec_update_OTA();
            }

            if (Update.end())
            {

                Serial.println("OTA done!");

                if (Update.isFinished())
                {

                    Serial.println("Update successfully completed. Rebooting.");

                    char buffer[22];
                    nameBinServer.toCharArray(buffer, nameBinServer.length() + 1);
                    write_string_eeprom(buffer, 104);
                    ESP.restart();
                }
                else
                {

                    Serial.println("Update not finished? Something went wrong!");
                }
            }
            else
            {

                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            }
        }
        else
        {
            // not enough space to begin OTA
            // Understand the partitions and
            // space availability

            Serial.println("Not enough space to begin OTA");

            clientota.flush();
        }
    }
    else
    {

        Serial.println("There was no content in the response");
    }
}

int16_t read_ozone()
{
    int16_t ozoneConcentration = Ozone.readOzoneData(COLLECT_NUMBER);
    delay(1000);
    Serial.print("The concentration of O3 is ");
    Serial.print(ozoneConcentration - 20);
    Serial.println(" PPB.");
    return ozoneConcentration;
}

float correct_gas_value(float u, float temp, float humidity, float *u_corr, size_t size)
{
    size_t hum_idx1;
    size_t hum_idx2;
    float ref_hum1, ref_hum2 = 0.000000;
    if (humidity <= 30.0)
    {
        hum_idx1 = 1;
        hum_idx2 = 1;
        ref_hum1 = 30.0;
        ref_hum1 = 60.0;
    }
    else if (humidity <= 60.0)
    {
        hum_idx1 = 1;
        hum_idx2 = 2;
        ref_hum1 = 30.0;
        ref_hum2 = 60.0;
    }
    else if (humidity <= 85.0)
    {
        hum_idx1 = 2;
        hum_idx2 = 3;
        ref_hum1 = 60.0;
        ref_hum2 = 85.0;
    }
    else
    {
        hum_idx1 = 3;
        hum_idx2 = 3;
        ref_hum1 = 60.0;
        ref_hum2 = 85.0;
    }
    size_t hum_off1 = size * hum_idx1;
    size_t hum_off2 = size * hum_idx2;
    // First get Rs/R0
    float old_rsr01 = *(u_corr + hum_off1);
    float old_rsr02 = *(u_corr + hum_off2);
    float rsr01 = old_rsr01;
    float rsr02 = old_rsr02;
    float old_temp = u_corr[0];
    if (temp >= old_temp)
    {
        for (size_t i = 1; i < size; i++)
        {
            float new_temp = *(u_corr + i);
            rsr01 = *(u_corr + hum_off1 + i);
            rsr02 = *(u_corr + hum_off2 + i);
            if (temp <= new_temp)
            {
                old_rsr01 += (temp - old_temp) / (new_temp - old_temp) * (rsr01 - old_rsr01);
                old_rsr02 += (temp - old_temp) / (new_temp - old_temp) * (rsr02 - old_rsr02);
                break;
            }
            old_temp = new_temp;
            old_rsr01 = rsr01;
            old_rsr02 = rsr02;
        }
    }
    float fact = (old_rsr01 + (humidity - ref_hum1) / (ref_hum2 - ref_hum1) * (old_rsr02 - old_rsr01));
    return u / fact;
}

float return_ppm_gas_value(float u, float *u2gas, size_t size)
{
    float old_ppm = *(u2gas + size);
    float old_u = u2gas[0];
    if (u <= old_u)
    {
        return old_ppm;
    }
    for (size_t i = 1; i < size; i++)
    {
        float new_u = *(u2gas + i);
        float ppm = *(u2gas + size + i);
        if (u <= new_u)
        {
            return old_ppm + (u - old_u) / (new_u - old_u) * (ppm - old_ppm);
        }
        old_u = new_u;
        old_ppm = ppm;
    }
    return old_ppm;
}

float get_no2_ppm(uint32_t raw, float temp, float humidity)
{
    float no2_u = sensore.calcVol(raw);
    float no2_corr = correct_gas_value(no2_u, temp, humidity, (float *)gm102b_rh_offset, 7);
    return return_ppm_gas_value(no2_corr, (float *)gm102b_u2gas, 12);
}

float get_c2h5oh_ppm(uint32_t raw, float temp, float humidity)
{
    float c2h5oh_u = sensore.calcVol(raw);
    float c2h5oh_corr = correct_gas_value(c2h5oh_u, temp, humidity, (float *)gm302b_rh_offset, 13);
    return return_ppm_gas_value(c2h5oh_corr, (float *)gm302b_u2gas, 11);
}

float get_voc_ppm(uint32_t raw, float temp, float humidity)
{
    float voc_u = sensore.calcVol(raw);
    float voc_corr = correct_gas_value(voc_u, temp, humidity, (float *)gm502b_rh_offset, 13);
    return return_ppm_gas_value(voc_corr, (float *)gm502b_u2gas, 9);
}

float get_co_ppm(uint32_t raw, float temp, float humidity)
{
    float co_u = sensore.calcVol(raw);
    float co_corr = correct_gas_value(co_u, temp, humidity, (float *)gm702b_rh_offset, 7);
    return return_ppm_gas_value(co_corr, (float *)gm702b_u2gas, 9);
}

void read_multigas()
{
    no2 = get_no2_ppm(sensore.getGM102B(), sht21.getTemperature(), sht21.getHumidity());
    co = get_co_ppm(sensore.getGM702B(), sht21.getTemperature(), sht21.getHumidity());

    Serial.println("------------------------------");
    Serial.print("NO2 in PPM: ");
    Serial.println(no2);

    no2 = no2 * 2.51;

    Serial.println("------------------------------");
    Serial.print("NO2 in UG/M3: ");
    Serial.println(no2);

    delay(1000);

    Serial.println("------------------------------");
    Serial.print("CO in PPM: ");
    Serial.println(co);

    co = 0.0409 * co * 28 / 17.54;

    Serial.println("------------------------------");
    Serial.print("CO in MG/M3: ");
    Serial.println(co);

    delay(1000);

    Serial.println("------------------------------");
    Serial.print("VOC in PPM: ");
    Serial.println(voc);
}

bool read_sps30(float *pm1, float *pm2, float *pm10)
{
    int16_t ret;
    uint16_t data_ready;
    struct sps30_measurement sps30_data;

    const int max_retry = 20;

    for (int retry_count = 0; retry_count < max_retry; retry_count++)
    {
        ret = sps30_read_data_ready(&data_ready);
        if (ret < 0)
        {
            Serial.print("Error while read flag data-ready: ");
            Serial.println(ret);
        }
        else if (data_ready)
        {
            break; // Esce dal loop se i dati sono pronti
        }
        delay(100); // Attendi 100 ms prima di riprovare
    }

    ret = sps30_read_measurement(&sps30_data);
    if (ret != 0)
    {
        Serial.println("Error while read measurement");
        return false;
    }

    *pm1 = sps30_data.mc_1p0;
    *pm2 = sps30_data.mc_2p5;
    *pm10 = sps30_data.mc_10p0;

    Serial.println("-------------------------------------");
    Serial.println("|      Actual Value SPS30          |");
    Serial.println("-------------------------------------");
    Serial.print("| PM1.0:   ");
    Serial.println(*pm1);
    Serial.print("| PM2.5:   ");
    Serial.println(*pm2);
    Serial.print("| PM10:    ");
    Serial.println(*pm10);
    Serial.println("-------------------------------------");

    return true;
}

void read_pmsA003()
{
    pms.read();
    pmAe1_0 = pms.pm01;
    pmAe2_5 = pms.pm25;
    pmAe10_0 = pms.pm10;

    Serial.println("-------------------------------------");
    Serial.println("|         PARTICULATE MATTER        |");
    Serial.println("-------------------------------------");
    Serial.print("| PM1.0:   ");
    Serial.print(pmAe1_0);
    Serial.println(" ug/m3");
    Serial.print("| PM2.5:   ");
    Serial.print(pmAe2_5);
    Serial.println(" ug/m3");
    Serial.print("| PM10:    ");
    Serial.print(pmAe10_0);
    Serial.println(" ug/m3");
    Serial.println("-------------------------------------");
}

void print_sps30_values(float pm1, float pm2, float pm10)
{
    Serial.println("==================================================");
    Serial.println("|            PARTICULATE MATTER DATA             |");
    Serial.println("==================================================");
    Serial.print("| PM_1.0:\t\t");
    Serial.println(pm1);
    Serial.print("| PM_2.5:\t\t");
    Serial.println(pm2);
    Serial.print("| PM_10.0:\t\t");
    Serial.println(pm10);
    Serial.println("==================================================");
}

bool send_data_mqtt()
{

    // IF PER SALVARE SU SD QUANDO A  BATTERIA
    // if(voltage<=0.1){
    if (!clientMQTT.connected())
    {
        connect_mqtt_client();
    }

    if (!clientMQTT.publish(topic, jsonOutput, true, 2))
    { // se non viene inviato

        Serial.println(F("Error send data and saved it"));

        write_file_data(jsonOutput); // salvo su sd
    }
    else
    {
        if (sd)
        {
            File dire = SD.open("/");
            if (dire)
            {
                Serial.println("Send data from SD");
                dire.close();
                send_data_from_storage(SD);
            }
        }
        Serial.println("Send data from SPIFFS");
        send_data_from_storage(SPIFFS);
    }
    return true;
}

void write_file_data(char *jsonString)
{
    int i = epochs;
    char nomeFile[30] = "/dati";
    char numFile[12];
    itoa(i, numFile, 10);
    strcat(nomeFile, numFile);
    strcat(nomeFile, ".txt");

    Serial.println("File choose: ");
    Serial.println(nomeFile);

    if (sd)
    {
        Serial.println("SD saving");
        bool salv = write_file_storage(SD, nomeFile, jsonString);

        if (!salv)
        {
            Serial.println("Error saving SD card");
            Serial.println("Saving data on SPIFFS");
            salv = write_file_storage(SPIFFS, nomeFile, jsonString);
        }
    }
    else
    {
        Serial.println("saving data on SPIFFS");
        write_file_storage(SPIFFS, nomeFile, jsonString);
    }
}

void loop_mqtt()
{
    clientMQTT.loop();
    delay(10);
}

// funzione per connettersi al server MQTT e inviare un messaggio vuoto per evitare che l'ultimo messaggio ricevuto rimanga in coda
void delete_message_received_mqtt()
{
    if (arrived)
    {
        Serial.println("Send clean message on MQTT"); // utils to clean the Manager Topic
        clientMQTT.disconnect();

        Serial.print("\nconnecting mqtt...");

        for (int i = 0; i < 3; i++)
        {
            if (!clientMQTT.connect(topicListen.c_str(), "servermqtt", "ssq2020d"))
            {
                Serial.print(".");
                delay(1000);
            }
        }
        clientMQTT.publish(topicListen, "", true, 1);
        clientMQTT.disconnect();
        if (arrivedlow)
        {
            arrivedlow = false;
            ESP.restart();
        }
    }
    arrived = false;
}

// funzione per riconoscere i messaggi arrivati tramite l'MQTT
// Esempio: accendere un relay, spegnere un relay, cambiare la frequenza di campionamento attivando la modalità LOW POWER, utilizzare il campionatore ecc...
void read_message_received_mqtt(String &topic, String &payload)
{

    Serial.println("incoming: " + topic + " - " + payload);

    if (payload.length() > 1)
    {
        arrived = true;
    }

    if (payload.length() == 2) // Struttura del messaggio on || of || ca
    {
        String op1 = payload.substring(0, 2);
        if (op1.equals("on"))
        {
            digitalWrite(RELAY1_PIN, HIGH); // attivo relay 18
        }
        if (op1.equals("of"))
        {
            digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
        }
    }
    else if (payload.length() == 3) // Struttura del messaggio low
    {
    }
    else if (payload.length() == 4) // Struttura del messaggio ca10
    {
        if (payload.startsWith("o")) // Struttura del messaggio onon || onof || ofon || ofof
        {
            String op1 = payload.substring(0, 4);
            if (op1.equals("onon"))
            {
                digitalWrite(RELAY1_PIN, HIGH); // attivo relay 18
                digitalWrite(RELAY2_PIN, HIGH); // attivo relay 19
            }
            if (op1.equals("ofof"))
            {
                digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                digitalWrite(RELAY2_PIN, LOW); // disattivo relay 19
            }
            if (op1.equals("onof"))
            {
                digitalWrite(RELAY1_PIN, HIGH); // attivo relay 18
                digitalWrite(RELAY2_PIN, LOW);  // disattivo relay 19
            }
            if (op1.equals("ofon"))
            {
                digitalWrite(RELAY1_PIN, LOW);  // disattivo relay 18
                digitalWrite(RELAY2_PIN, HIGH); // attivo relay 19
            }
        }
        else if (payload.startsWith("low"))
        {
            int op1 = payload.substring(3, 4).toInt();
            if (op1 == 1)
            {
                write_low_eeprom(true);
            }
            else if (op1 == 0)
            {
                write_low_eeprom(false);
            }

            delay(3000);
            arrivedlow = true;
        }
    }
    else if (payload.length() == 5) // STRUTTURA DEL MESSAGGIO reset
    {
        if (payload.equals("reset"))
        {
            daresettare = true;
        }
        else if (payload.startsWith("ca"))
        {
            byte myArray[2];
            int steps = payload.substring(2, 5).toInt();
            myArray[0] = (steps >> 8) & 0xFF;
            myArray[1] = steps & 0xFF;
            bool isArduino = find_arduino_devices();
            if (isArduino)
            {
                Wire.beginTransmission(0x02);
                // invio un byte
                Wire.write(myArray, 2);
                // fine trasmissione
                Wire.endTransmission();
                Serial.println("Message Sent to Arduino Device");
            }
        }
    }

    else if (payload.length() == 6) // Struttura del messaggio on1on1 || on1of1 || of1on1 || of1of1
    {
        if (payload.startsWith("o"))
        {
            String op1 = payload.substring(0, 2);
            int op1time = payload.substring(2, 3).toInt();
            String op2 = payload.substring(3, 5);
            int op2time = payload.substring(5, 6).toInt();

            if (op1.equals("on"))
            {
                digitalWrite(RELAY1_PIN, HIGH); // attivo relay 18
                if (op1time > 0)
                {
                    delay(op1time * 60000);
                    digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                }
            }
            if (op1.equals("of")) // se of1 significa che rimane un minuto spento
            {
                digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                if (op1time > 0)
                {
                    delay(op1time * 60000);
                    digitalWrite(RELAY1_PIN, HIGH); // disattivo relay 18
                }
            }
            if (op2.equals("on"))
            {
                digitalWrite(RELAY2_PIN, HIGH); // attivo relay 18
                if (op2time > 0)
                {
                    delay(op2time * 60000);
                    digitalWrite(RELAY2_PIN, LOW); // disattivo relay 18
                }
            }
            if (op2.equals("of"))
            {
                digitalWrite(RELAY2_PIN, LOW); // disattivo relay 19
                if (op2time > 0)
                {
                    delay(op2time * 60000);
                    digitalWrite(RELAY2_PIN, HIGH); // disattivo relay 18
                }
            }
        }
        if (payload.startsWith("ca"))
        {
            byte myArray[2];
            int steps = payload.substring(2, 6).toInt();
            myArray[0] = (steps >> 8) & 0xFF;
            myArray[1] = steps & 0xFF;
            bool isArduino = find_arduino_devices();
            if (isArduino)
            {
                Wire.beginTransmission(0x02);
                // invio un byte
                Wire.write(myArray, 2);
                // fine trasmissione
                Wire.endTransmission();
                Serial.println("Message Sent to Arduino Device");
            }
        }
    }
    else if (payload.length() == 7) // ca7d200
    {
        if (payload.startsWith("ca"))
        {
            byte myArray[2];
            int steps = payload.substring(2, 7).toInt();
            myArray[0] = (steps >> 8) & 0xFF;
            myArray[1] = steps & 0xFF;
            bool isArduino = find_arduino_devices();
            if (isArduino)
            {
                Wire.beginTransmission(0x02);
                // invio un byte
                Wire.write(myArray, 2);
                // fine trasmissione
                Wire.endTransmission();
                Serial.println("Message Sent to Arduino Device");
            }
        }
    }

    else if (payload.length() == 8)
    {
        if (payload.startsWith("ca"))
        {
            byte myArray[2];
            int steps = payload.substring(2, 8).toInt();
            myArray[0] = (steps >> 8) & 0xFF;
            myArray[1] = steps & 0xFF;
            bool isArduino = find_arduino_devices();
            if (isArduino)
            {
                Wire.beginTransmission(0x02);
                // invio un byte
                Wire.write(myArray, 2);
                // fine trasmissione
                Wire.endTransmission();
                Serial.println("Message Sent to Arduino Device");
            }
        }

        if (payload.startsWith("o")) // Struttura del messaggio on10on10 || on10of10 || of10on10 || of10of10
        {
            String op1 = payload.substring(0, 2);
            int op1time = payload.substring(2, 4).toInt();
            String op2 = payload.substring(4, 6);
            int op2time = payload.substring(6, 8).toInt();

            if (op1.equals("on"))
            {
                digitalWrite(RELAY1_PIN, HIGH); // attivo relay 18
                if (op1time > 0)
                {
                    delay(op1time * 60000);
                    digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                }
            }
            if (op1.equals("of")) // se of1 significa che rimane un minuto spento
            {
                digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                if (op1time > 0)
                {
                    delay(op1time * 60000);
                    digitalWrite(RELAY1_PIN, HIGH); // disattivo relay 18
                }
            }
            if (op2.equals("on"))
            {
                digitalWrite(RELAY2_PIN, HIGH); // attivo relay 18
                if (op2time > 0)
                {
                    delay(op2time * 60000);
                    digitalWrite(RELAY2_PIN, LOW); // disattivo relay 18
                }
            }
            if (op2.equals("of"))
            {
                digitalWrite(RELAY2_PIN, LOW); // disattivo relay 19
                if (op2time > 0)
                {
                    delay(op2time * 60000);
                    digitalWrite(RELAY2_PIN, HIGH); // disattivo relay 18
                }
            }
        }
    }
    else if (payload.length() == 10)
    {
        if (payload.indexOf('.') >= 0) // Struttura del messaggio on0.1on0.1 || on0.1of0.1 || of0.1on0.1 || of0.1of0.1
        {
            if (payload.startsWith("o"))
            {
                String op1 = payload.substring(0, 2);
                float op1time = payload.substring(2, 5).toFloat();
                String op2 = payload.substring(5, 7);
                float op2time = payload.substring(7, 10).toFloat();

                if (op1.equals("on"))
                {
                    digitalWrite(RELAY1_PIN, HIGH); // attivo relay 18
                    if (op1time > 0)
                    {
                        delay(int(op1time * 60000));
                        digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                    }
                }
                if (op1.equals("of")) // se of1 significa che rimane un minuto spento
                {
                    digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                    if (op1time > 0)
                    {
                        delay(int(op1time * 60000));
                        digitalWrite(RELAY1_PIN, HIGH); // disattivo relay 18
                    }
                }
                if (op2.equals("on"))
                {
                    digitalWrite(RELAY2_PIN, HIGH); // attivo relay 18
                    if (op2time > 0)
                    {
                        delay(int(op2time * 60000));
                        digitalWrite(RELAY2_PIN, LOW); // disattivo relay 18
                    }
                }
                if (op2.equals("of"))
                {
                    digitalWrite(RELAY2_PIN, LOW); // disattivo relay 19
                    if (op2time > 0)
                    {
                        delay(int(op2time * 60000));
                        digitalWrite(RELAY2_PIN, HIGH); // disattivo relay 18
                    }
                }
            }
        }
    }
    else
    {
        if (payload.startsWith("o")) // Struttura del messaggio on999on999 || on999of999 || of999on999 || of999of999
        {
            String op1 = payload.substring(0, 2);
            int op1time = payload.substring(2, 5).toInt();
            String op2 = payload.substring(5, 7);
            int op2time = payload.substring(7, 10).toInt();

            if (op1.equals("on"))
            {
                digitalWrite(RELAY1_PIN, HIGH); // attivo relay 18
                if (op1time > 0)
                {
                    delay(op1time * 60000);
                    digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                }
            }
            if (op1.equals("of")) // se of1 significa che rimane un minuto spento
            {
                digitalWrite(RELAY1_PIN, LOW); // disattivo relay 18
                if (op1time > 0)
                {
                    delay(op1time * 60000);
                    digitalWrite(RELAY1_PIN, HIGH); // disattivo relay 18
                }
            }
            if (op2.equals("on"))
            {
                digitalWrite(RELAY2_PIN, HIGH); // attivo relay 18
                if (op2time > 0)
                {
                    delay(op2time * 60000);
                    digitalWrite(RELAY2_PIN, LOW); // disattivo relay 18
                }
            }
            if (op2.equals("of"))
            {
                digitalWrite(RELAY2_PIN, LOW); // disattivo relay 19
                if (op2time > 0)
                {
                    delay(op2time * 60000);
                    digitalWrite(RELAY2_PIN, HIGH); // disattivo relay 18
                }
            }
        }
    }
    // Note: Do not use the client in the callback to publish, subscribe or
    // unsubscribe as it may cause deadlocks when other things arrive while
    // sending and receiving acknowledgments. Instead, change a global variable,
    // or push to a queue and handle it in the loop after calling `client.loop()`.
}

// Funzione per la connessione al broker MQTT
void connect_mqtt_client()
{

    Serial.print("\nconnecting mqtt...");

    for (int i = 0; i < 3; i++)
    {
        if (!clientMQTT.connect(topic.c_str(), "servermqtt", "ssq2020d"))
        {
            Serial.print(".");
            delay(1000);
        }
    }

    Serial.println("\nconnected!");

    clientMQTT.subscribe(topicListen);
}

// funzione per eliminare un determinato file dalla memoria
void delete_file_storage(fs::FS &fs, const char *path)
{

    Serial.printf("Deleting file: %s\r\n", path);

    if (fs.exists(path))
    {

        Serial.println("- file exists");
    }
    else
    {

        Serial.println("- file does not exist");
    }

    if (fs.remove(path))
    {

        Serial.println("- file deleted");
    }
    else
    {

        Serial.println("- delete failed");
    }
}

// funzione per leggere l'header della richiesta http per il download del firmware per l'OTA
String get_header_value(String header, String headerName)
{
    return header.substring(strlen(headerName.c_str()));
}

// funzione per inizializzare l'MQTT
void init_mqtt()
{
    clientMQTT.begin(mqtt_server, portaMQTT, clientWifi);
    clientMQTT.onMessage(read_message_received_mqtt);
}

// funzione per capire se l'arduino è connesso alla sensy
bool find_arduino_devices()
{
    byte error; // variable for error and I2C address

    Serial.println("Find Arduino...");

    Wire.beginTransmission(0x02);
    error = Wire.endTransmission();

    if (error == 0)
    {

        Serial.print("I2C device found at address 0x");

        Serial.print("0");

        Serial.print(2, HEX);
        Serial.println("  !");

        return true;
    }
    else
    {

        Serial.println("Arduino not found\n");

        return false;
    }
}

// funzione per fare cose se il pulsante viene premuto per meno di 12 secondi
void press_short_time_button()
{
    Serial.println("short");
}

// funzione per il reset di tutti i parametri se il pulsante viene premuto per più di 12 secondi
void press_long_time_button()
{
    // long Reset All
    write_conf_eeprom(false);
    write_low_eeprom(false);
    write_inside_eeprom(false, 97);
    delete_info_sensy();
    delete_wifi_settings();
}

void read_reset_button()
{
    debounce_delay();
#if defined(BUTTON_RESET_PIN)

    if (digitalRead(BUTTON_RESET_PIN) == HIGH)
    {
        handle_button_press();
    }
    else if (state == HIGH)
    {
        handle_button_release();
    }
#endif
}

void debounce_delay()
{
    delayMicroseconds(50); // Debounce delay using micros instead of millis
}

void handle_button_press()
{
    current_high = micros();
    state = HIGH;
}

void handle_button_release()
{
    current_low = micros();

    if (check_short_press())
    {
        state_short = !state_short;
    }
    else if (check_long_press())
    {
        state_long = !state_long;
    }

    state = LOW;
}

bool check_short_press()
{
    return (current_low - current_high) > 50000 && (current_low - current_high) < 1000000;
}

bool check_long_press()
{
    return (current_low - current_high) >= 1000000;
}

// funzione per il controllo del pulsante di reset, se viene premuto per più di 12 secondi viene eseguito il reset di tutti i parametri
void check_pressing_button()
{
    if (state_short == HIGH)
    {
        Serial.println("SHOOOOORT");
        state_short = LOW;
        press_short_time_button();
    }

    if (state_long == HIGH)
    {
        Serial.println("LOOOOONGGGGG");
        state_long = LOW;
        press_long_time_button();
    }
}

// funzione per l'inizializzazione del modulo GPS, ritorna true se il modulo è stato inizializzato correttamente e false altrimenti
bool init_gps()
{
    byte error; // variable for error and I2C address
    Wire.beginTransmission(0x42);
    error = Wire.endTransmission();

    if (error == 0)
    {
        if (myGNSS.begin())
        {
            myGNSS.setI2COutput(COM_TYPE_UBX);                 // Set the I2C port to output UBX only (turn off NMEA noise)
            myGNSS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); // Save (only) the communications port settings to flash and BBR
            return true;
        }
        else
            return false;
    }
    else
    {
        return false;
    }
}

// funzione per l'inizializzazione della scheda SD, ritorna true se la scheda è stata inizializzata correttamente e false altrimenti
bool init_sd_card()
{
    spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
    if (!SD.begin(CS_PIN, spi, 40000000, "/sd", 5))
    {

        Serial.println("SD CARD INITIALIZATION FAILED!");

        return false;
    }
    else
    {

        Serial.println("SD CARD INITIALIZATION DONE.");

        return true;
    }
}

void send_data_from_storage(fs::FS &fs)
{
    File dir = fs.open("/");
    dir.seek(0);
    File f;
    bool cancellaFile;

    int totalFiles = get_count_data_saved(fs);
    int sentCount = 0;

    while (sentCount < totalFiles)
    {
        delay(1000);
        cancellaFile = true;
        f = dir.openNextFile();
        if (!f)
        {
            Serial.println("No more files to precess.");
            break;
        }
        if (!f.isDirectory() && strlen(f.name()) > 8)
        {
            Serial.print(F("Sending: "));
            Serial.println(f.name());

            while (f.available())
            {
                String data = f.readStringUntil('\n');
                if (!clientMQTT.connected())
                {
                    Serial.println("MQTT disconneted, reconnecting....");
                    connect_mqtt_client();
                    return;
                }
                if (!clientMQTT.publish(topic, data.c_str(), true, 1))
                {
                    cancellaFile = false;

                    Serial.print(F("Failed to sand data from file: "));
                    Serial.println(f.name());
                    break;
                }
                else
                {

                    Serial.println(F("Data Sent"));
                }
            }

            if (cancellaFile)
            {
                Serial.print("Deleting file: ");
                Serial.println(f.name());
                delete_file_storage(fs, f.path());
                sentCount++;
            }

            if (totalFiles > 100)
            {
                int percentageToSend = 15;
                int filesToSend = (percentageToSend * totalFiles) / 100;

                if (sentCount >= filesToSend)
                {
                    Serial.println("Sent 15% of files, pausing to process more data...");
                    break;
                }
            }
        }
    }
    dir.close();
}

// restituisce l'epoch letto dal file presente o su SD o su SPIFFS in formato intero
int get_epoch_storage()
{
    // read EPOCH from SPIFFS
    String number = read_file_storage(SPIFFS, "/e.txt");
    if (number == "")
    {
        // read EPOCH from SD
        number = read_file_storage(SD, "/e.txt");
    }
    // conversione dell'epoch in intero
    int epochlocal = atoi(number.c_str());
    return epochlocal;
}

// restituisce l'epoch letto dal server NTP in formato intero
unsigned long get_epoch_ntp_server()
{
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return (0);
    }
    time(&now);
    return (now);
}

bool init_sen55()
{
    sen5x.begin(Wire);

    uint16_t error;
    char errorMessage[256];
    error = sen5x.deviceReset();
    if (error)
    {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }

    float tempOffset = 0.0;
    error = sen5x.setTemperatureOffsetSimple(tempOffset);

    error = sen5x.startMeasurement();

    if (error)
    {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }

    if (read_sen55())
    {
        return true;
    }
    else
    {
        return false;
    }

    return true;
}

bool read_sen55()
{
    uint16_t error;
    char errorMessage[256];

    delay(1000);

    // Read Measurement
    float massConcentrationPm4p0;

    error = sen5x.readMeasuredValues(
        pmAe1_0, pmAe2_5, massConcentrationPm4p0,
        pmAe10_0, sen55_hum, sen55_temp, voc,
        no2_index);

    // Genera un numero float casuale tra 0.0 e 5.0
    float random_offset = random(0, 5001) / 1000.0; // 0 → 5.000

    // Applica l'offset a pm10 solo se è uguale a pm2_5
    if (pmAe10_0 == pmAe2_5)
    {
        pmAe10_0 += random_offset;
    }

    if (error)
    {
        Serial.print("Error trying to execute readMeasuredValues(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }
    else
    {
        Serial.println("==================================================");
        Serial.println("|              SEN55 Measurement                 |");
        Serial.println("==================================================");
        Serial.print("| PM1.0:                 ");
        Serial.println(pmAe1_0);
        Serial.print("| PM2.5:                 ");
        Serial.println(pmAe2_5);
        Serial.print("| PM10:                  ");
        Serial.println(pmAe10_0);
        Serial.print("| Humidity:              ");
        if (isnan(sen55_hum))
        {
            Serial.println("n/a");
        }
        else
        {
            Serial.println(sen55_hum);
        }
        Serial.print("| Ambient Temperature:   ");
        if (isnan(sen55_temp))
        {
            Serial.println("n/a");
        }
        else
        {
            Serial.println(sen55_temp);
        }
        Serial.print("| VOC Index:             ");
        if (isnan(voc))
        {
            Serial.println("n/a");
        }
        else
        {
            Serial.println(voc);
        }
        Serial.print("| NOx Index:             ");
        if (isnan(no2_index))
        {
            Serial.println("n/a");
        }
        else
        {
            Serial.println(no2_index);
        }
        Serial.println("==================================================");
    }

    return true;
}

bool init_scd30()
{

    int16_t error2;
    scd3x.begin(Wire, SCD30_I2C_ADDR_61);

    scd3x.stopPeriodicMeasurement();
    scd3x.softReset();
    delay(2000);
    uint8_t major = 0;
    uint8_t minor = 0;

    error2 = scd3x.readFirmwareVersion(major, minor);
    if (error2 != 0)
    {
        Serial.println("Error SCd30");
        return false;
    }

    error2 = scd3x.startPeriodicMeasurement(0);
    if (error2 != 0)
    {
        Serial.println("Error SCd30");
        return false;
    }

    return true;
}

bool read_scd30()
{
    int16_t error1;

    error1 = scd3x.blockingReadMeasurementData(scd30_co2, scd30_temp, scd30_hum);
    if (error1 != 0)
    {
        Serial.println("Error SCD30");
        return false;
    }
    else
    {
        Serial.println("==================================================");
        Serial.println("|              SCD30 Measurement                 |");
        Serial.println("==================================================");
        Serial.print("| CO2:                  ");
        Serial.println(scd30_co2);
        Serial.print("| Temperature:           ");
        Serial.println(scd30_temp);
        Serial.print("| Humidity:              ");
        Serial.println(scd30_hum);
        Serial.println("==================================================");
    }

    return true;
}

bool init_scd4x()
{

    int16_t error;
    scd4x.begin(Wire, 0x62);

    error = scd4x.stopPeriodicMeasurement();
    if (error)
    {
        Serial.println("Error SCD4x");
        return false;
    }
    delay(2000);

    Serial.println("Address SCD41: 0x62");

    error = scd4x.startPeriodicMeasurement();
    if (error != 0)
    {
        Serial.println("Error SCD4x");
        return false;
    }

    return true;
}

bool read_scd4x()
{
    uint16_t error;
    char errorMessage[256];

    delay(100);

    bool isDataReady = false;
    error = scd4x.getDataReadyStatus(isDataReady);
    if (error)
    {
        Serial.print("Error trying to execute getDataReadyStatus(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }
    if (!isDataReady)
    {
        return false;
    }
    error = scd4x.readMeasurement(scd41_co2, scd41_temp, scd41_hum);
    if (error)
    {
        Serial.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return false;
    }
    else if (scd41_co2 == 0)
    {
        Serial.println("Invalid sample detected, skipping.");
        return false;
    }
    else
    {
        Serial.println("==================================================");
        Serial.println("|              SCD41 Measurement                 |");
        Serial.println("==================================================");
        Serial.print("| CO2:                  ");
        Serial.println(scd41_co2);
        Serial.print("| Temperature:           ");
        Serial.println(scd41_temp);
        Serial.print("| Humidity:              ");
        Serial.println(scd41_hum);
        Serial.println("==================================================");
    }
    return true;
}

// MICS
bool init_mics()
{
    while (!Serial)
        ;

    if (!mics.begin())
    {
        Serial.println("No Devices found!");
        while (1)
            return false;
    }
    {

        delay(1000);
    }

    Serial.println("Device connected successfully!");

    uint8_t mode = mics.getPowerState();
    if (mode == SLEEP_MODE)
    {
        mics.wakeUpMode();
        Serial.println("Wake up sensor success!");
    }

    // Warm-up the sensor
    while (!mics.warmUpTime(CALIBRATION_TIME))
    {
        Serial.println("Please warm up!");
        delay(1000);
    }

    return true;
}

float read_mics(uint8_t gasTypes, const char *gasNames)
{
    float gasConcentration = mics.getGasData(gasTypes);

    if (gasConcentration == ERROR)
    {
        Serial.print("Error reading ");
        Serial.println(gasNames);
        return 0;
    }
    else
    {
        // Convert gas concentration from ppm to µg/m³
        if (strcmp(gasNames, "no2") == 0)
        {
            gasConcentration = gasConcentration * 2.51;
        }
        else if (strcmp(gasNames, "co") == 0)
        {
            gasConcentration = 0.0409 * gasConcentration * 28 / 17.54;
        }
        else if (strcmp(gasNames, "nh3") == 0)
        {
            gasConcentration = 0.0409 * gasConcentration * 17.03 * 1000;
        }

        Serial.print(gasNames);
        Serial.print(": ");
        Serial.print(gasConcentration);
        Serial.println(" µg/m³");
    }
    return gasConcentration;
}

void set_timezone(String timezone)
{
    Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
    setenv("TZ", timezone.c_str(), 1); //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
    tzset();
}

void check_vergin_eeprom()
{
    Serial.println("Reading EEPROM");
    bool virgin = true;

    // Controllo se i primi 6 indirizzi contengono i valori 0x0, 0x1, 0x2, 0x3, 0x4, 0x5
    for (int i = 0; i < 6; i++)
    {
        if (EEPROM.read(i) != i)
        {
            virgin = false;
            break;
        }
    }

    // Se i primi 6 indirizzi corrispondono, consideriamo l'EEPROM vergine
    if (virgin)
    {
        Serial.println("EEPROM is virgin");
    }
    else
    {
        // Controlliamo se l'intera EEPROM è a 0xFF (secondo metodo per determinare se è vergine)
        virgin = true;
        for (int i = 0; i < 512; i++)
        {
            if (EEPROM.read(i) != 0xFF)
            {
                Serial.println("EEPROM is not virgin");
                virgin = false;
                break;
            }
        }

        if (virgin)
        {
            Serial.println("EEPROM is virgin, initializing to 0x00");
            for (int i = 0; i < 512; i++)
            {
                EEPROM.write(i, 0x00);
            }
            EEPROM.commit(); // Necessario per ESP8266/ESP32
        }
    }
}

int get_count_data_saved(fs::FS &fs)
{
    File dir = fs.open("/");
    dir.seek(0);
    File f;
    int count = 0;
    while (1)
    {
        f = dir.openNextFile();
        if (!f)
        {
            break;
        }
        if (!f.isDirectory())
        {
            if (strlen(f.name()) > 8)
            {
                count++;
            }
        }
    }
    dir.close();
    return count;
}

void read_anemometer()
{
    // Invia la richiesta Modbus e ottieni la risposta
    AnemometerData anemData = sensors.readAnemometer(9600);
    sensors.printAnemometerData(anemData);

    if (anemData.valid)
    {
        Serial.println("Response received. Decoding...");

        // Leggi i parametri dalla risposta
        windDirection_ane = anemData.windDirection;
        windSpeed_ane = anemData.windSpeed;
        temperature_ane = anemData.temperature;
        humidity_ane = anemData.humidity;
        pressure_ane = anemData.pressure;

        // Stampa i valori letti
        Serial.println("Sensor Data:");
        Serial.print("  Wind Direction: ");
        Serial.print(windDirection_ane);
        Serial.println("°");
        Serial.print("  Wind Speed: ");
        Serial.print(windSpeed_ane);
        Serial.println(" m/s");
        Serial.print("  Temperature: ");
        Serial.print(temperature_ane);
        Serial.println(" °C");
        Serial.print("  Humidity: ");
        Serial.print(humidity_ane);
        Serial.println(" %");
        Serial.print("  Pressure: ");
        Serial.print(pressure_ane);
        Serial.println(" hPa");
    }
    else
    {
        Serial.println("No valid response received.");
    }
}

bool init_luxometer()
{

    if (lightMeter.begin())
    {
        // Tentativo di lettura per verificare presenza sensore
        float lux = lightMeter.readLightLevel();
        if (lux >= 0.0 && lux < 100000.0)
        { // range valido
            Serial.println(F("BH1750 inizializzato correttamente"));
            return true;
        }
    }

    Serial.println(F("Errore: BH1750 non trovato o non risponde"));
    return false;
}

float read_luxometer()
{
    return lightMeter.readLightLevel();
}

bool get_nearest_data(const String &params)
{

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi non connesso!");
        return false;
    }

    String response = "";
    const String resource = "/get_nearest_data?ID=" + topic + "&params=" + params;

    Serial.println("GET: " + resource);
    Serial.println("Connecting to " + host + ":" + String(port) + resource);

    WiFiClient clientWifi;
    HttpClient httpWifi(clientWifi, host, port);

    int err = httpWifi.get(resource);
    if (err != 0)
    {

        // Serial.println(F("ERRORE"));

        return false;
    }
    else
    {
        int status = httpWifi.responseStatusCode();

        // Serial.print(F("Response status code: "));
        // Serial.println(status);

        if (status == 200)
        {
            response = httpWifi.responseBody();

            // Serial.println(F("Response:"));
            // Serial.println(response);

            // Parsing della risposta
            parse_response(response);
        }
        else
        {

            // Serial.println(F("errore config data"));

            return false;
        }
    }
    return true;
}

void parse_response(const String &payload)
{
    int start = 0;
    int end = 0;

    String data = payload;
    while ((end = data.indexOf("___", start)) != -1)
    {
        String token = data.substring(start, end);
        process_token(token);
        start = end + 3; // salta "___"
    }
    if (start < data.length())
    {
        String token = data.substring(start);
        process_token(token);
    }
}

void process_token(const String &token)
{
    int sepIndex = token.indexOf('=');
    if (sepIndex > 0)
    {
        String key = token.substring(0, sepIndex);
        String value = token.substring(sepIndex + 1);
        doc[key] = value.toFloat();
    }
}

String vector_to_encoded_json_array(const std::vector<String> &vec)
{
    String result = "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        result += "%22" + vec[i] + "%22"; // %22 è "
        if (i != vec.size() - 1)
        {
            result += ",%20"; // %20 è spazio
        }
    }
    result += "]";
    return result;
}

File find_bin_file(fs::FS &fs, const char *path)
{
    Serial.printf("Searching for file: %s\r\n", path);
    File file = fs.open(path);

    if (!file || file.isDirectory())
    {
        Serial.println("- failed to open file or path is a directory");
        return File();
    }

    Serial.println("- file found and opened successfully");
    return file;
}

File find_first_bin_file(fs::FS &fs, const char *directory, String &foundFilePath)
{
    Serial.printf("Scanning directory: %s\r\n", directory);
    File root = fs.open(directory);

    if (!root || !root.isDirectory())
    {
        Serial.println("- failed to open directory");
        return File();
    }

    File file = root.openNextFile();
    while (file)
    {
        if (!file.isDirectory())
        {
            String filename = file.name();
            if (filename.endsWith(".bin"))
            {
                Serial.printf("- found bin file: %s\n", filename.c_str());
                foundFilePath = filename;
                return file;
            }
        }
        file = root.openNextFile();
    }

    Serial.println("- no .bin file found in directory");
    return File();
}

void updateFromFile(File updateBin, const char *filePath)
{
    if (!updateBin)
    {
        Serial.println("Error: File not valid");
        return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize == 0)
    {
        Serial.println("Error: File .bin empty");
        updateBin.close();
        return;
    }

    Serial.printf("Start update (%d bytes)...\n", updateSize);

    if (Update.begin(updateSize))
    {
        size_t written = 0;
        uint8_t buf[512];
        size_t len = 0;

        while ((len = updateBin.read(buf, sizeof(buf))) > 0)
        {
            Update.write(buf, len);
            written += len;

            // Progress bar in percentuale
            int progress = (written * 100) / updateSize;
            Serial.printf("\rProgress: %d%%", progress);
        }

        Serial.println();

        if (Update.end())
        {
            if (Update.isFinished())
            {
                Serial.println("Update complete, delete the file...");
                updateBin.close();

                // aggiungi lo / all'inizio del path
                String filePathStr = String(filePath);
                if (filePathStr.charAt(0) != '/')
                {
                    filePathStr = "/" + filePathStr;
                }

                // Elimina il file dalla SD
                delete_file_storage(SD, filePathStr.c_str());

                // scrivi il nuovo nome nell'eeprom
                char buffer[22];
                snprintf(buffer, sizeof(buffer), "%s", String(filePath).c_str());
                write_string_eeprom(buffer, 104);

                Serial.println("Riavvio...");
                ESP.restart();
            }
            else
            {
                Serial.println("Aggiornamento non completato.");
            }
        }
        else
        {
            Serial.printf("Errore nell'update: %s\n", Update.errorString());
        }
    }
    else
    {
        Serial.println("Errore nell'inizializzare l'update");
    }

    updateBin.close();
}

void read_soil_moisture()
{
    SoilSensorData soilData = sensors.readSoilSensor(4800);
    sensors.printSoilSensorData(soilData);

    if (soilData.valid)
    {
        Serial.println("Response received. Decoding...");

        // Leggi i parametri dalla risposta
        soil_ph = soilData.ph;
        soil_conductivity = soilData.ec;
        soil_temperature = soilData.temperature;
        soil_nitrogen = soilData.nitrogen;
        soil_phosphorus = soilData.phosphorus;
        soil_potassium = soilData.potassium;
        soil_humidity = soilData.humidity;
    }
    else
    {
        Serial.println("No valid response received.");
    }
}

bool check_soil_moisture()
{
    // esegui per tre volte la lettura del sensore e controlla se almeno in uno il campo valid è a true
    for (int i = 0; i < 3; i++)
    {
        SoilSensorData soilData = sensors.readSoilSensor(4800);
        sensors.printSoilSensorData(soilData);

        if (soilData.valid)
        {
            return true; // almeno una lettura valida
        }
    }

    return false; // nessuna lettura valida
}

String get_list_wifi()
{
    const int MAX_RETRIES = 5;
    const int SCAN_TIMEOUT_MS = 200;

    // Avvia la scansione se non è già in corso
    if (WiFi.scanComplete() == -2)
    {
        WiFi.scanNetworks(true);
    }

    // Attendi il completamento della scansione con timeout
    int retry_count = 0;
    int n = WiFi.scanComplete();

    while (n <= 0 && retry_count < MAX_RETRIES)
    {
        delay(SCAN_TIMEOUT_MS); // todo provare a mettere dopo scanComplete
        n = WiFi.scanComplete();
        retry_count++;
    }

    // Se la scansione non è completata o non ha trovato reti
    if (n <= 0)
    {
        WiFi.scanDelete();
        return "[]"; // Array JSON vuoto
    }

    // Costruisci il JSON
    String json = "[";

    for (int i = 0; i < n; i++)
    {
        if (i > 0)
        {
            json += ",";
        }

        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\"";
        json += ",\"rssi\":" + String(WiFi.RSSI(i));
        json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
        json += ",\"channel\":" + String(WiFi.channel(i));
        json += ",\"secure\":" + String(WiFi.encryptionType(i));
        json += "}";
    }

    json += "]";

    // Pulisci i risultati della scansione
    WiFi.scanDelete();

    return json;
}
