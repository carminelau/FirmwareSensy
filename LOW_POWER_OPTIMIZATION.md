# Suggerimenti di Ottimizzazione Energetica - Low Power Mode

## 📋 Status Implementazione Corrente

La modalità LOW POWER è strutturata nel seguente modo:
- **Cicli di lettura**: ogni 5 cicli (5 minuti)
- **Cicli di invio**: ogni 10 cicli (10 minuti)
- **Sniffer**: attivo SOLO nei cicli di invio
- **MQTT**: ascolta SOLO nei cicli di invio
- **CPU**: 10MHz durante sleep, 80MHz durante lavoro, 240MHz per operazioni critiche
- **Sensori**: skippati nei cicli non-lettura (mantengono ultimi dati validi)

**Consumo Stimato**:
- Modalità lettura: ~150-200mA
- Modalità sleep: ~10-20mA
- Media ogni 10 min: ~(2min × 175mA + 8min × 15mA) = ~122mA (vs 175mA normale)
- **Risparmio**: ~30-35% energia

---

## 🔋 Opzioni Aggiuntive di Ottimizzazione

### 1️⃣ **WiFi Radio Off tra i Cicli di Invio** (Impatto: 40-60mA risparmiati)

**Descrizione**: Spegnere il ricetrasmettitore WiFi tra i cicli di invio, riaccenderlo solo quando necessario.

**Implementazione**:
```cpp
// Prima del ciclo di skip
if (low && !isSendCycle) {
    WiFi.mode(WIFI_OFF);      // Spegni WiFi completamente
    Serial.printf("[LOW-POWER] WiFi spento - risparmi energetici attivati\n");
}

// Nei cicli di invio
if (low && isSendCycle) {
    WiFi.mode(WIFI_STA);       // Riaccendi WiFi
    WiFi.begin(ssid, password);
    // Aspetta connessione...
    unsigned long wifiTimeout = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiTimeout < 15000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Vantaggi**:
- Risparmio massimo nella modalità deep sleep (40-60mA)
- Nessun impatto sulla reattività (WiFi riattivato prima dell'invio)

**Svantaggi**:
- Comando MQTT non viene ricevuto istantaneamente nei cicli skip
- Riconnessione WiFi richiede 5-10 secondi

---

### 2️⃣ **Controllo Alimentazione dei Sensori** (Impatto: 50-100mA risparmiati se supportato)

**Descrizione**: Disalimentare fisicamente i sensori durante i cicli di skip.

**Implementazione** (se hardware supporta):
```cpp
#define SENSOR_POWER_PIN 5  // GPIO per relay potenza sensori

if (low && !isReadCycle) {
    digitalWrite(SENSOR_POWER_PIN, LOW);   // Spegni alimentazione sensori
    Serial.printf("[LOW-POWER] Sensori disalimentati\n");
}

if (low && isReadCycle) {
    digitalWrite(SENSOR_POWER_PIN, HIGH);  // Riaccendi alimentazione
    vTaskDelay(pdMS_TO_TICKS(2000));       // Aspetta stabilizzazione
    // Esegui letture sensori
}
```

**Vantaggi**:
- Massimo risparmio energetico per sensori ad alta potenza
- Sensori sempre in reset, inizializzazione pulita ad ogni ciclo

**Svantaggi**:
- Richiede hardware specifico (relay/MOSFET)
- Necessita calibrazione sensori ad ogni ciclo
- Aumenta inerzialità termica di sensori di temperatura

---

### 3️⃣ **CPU Frequency Scaling Più Aggressivo** (Impatto: 5-15mA risparmiati)

**Descrizione**: Usare frequenze CPU ancora più basse durante il deep sleep.

**Implementazione**:
```cpp
// Durante cicli di skip in low power
if (low && !isReadCycle) {
    setCpuFrequencyMhz(10);   // Attualmente 10MHz
    // Alternativa ancora più aggressiva:
    // setCpuFrequencyMhz(5);    // 5MHz - WARNING: alcuni I/O potrebbero non funzionare
}

// Durante cicli di lettura
if (isReadCycle) {
    setCpuFrequencyMhz(80);    // 80MHz per letture sensori
}

// Durante MQTT/WiFi
if (isSendCycle) {
    setCpuFrequencyMhz(240);   // 240MHz per comunicazione veloce
}
```

**Vantaggi**:
- Ulteriore riduzione consumo CPU
- Semplice da implementare

**Svantaggi**:
- Frequenze molto basse (5MHz) potrebbero causare problemi I2C/SPI
- Riduzione minima se il consumo è dominato da WiFi/sensori

---

### 4️⃣ **Sleep Interrupt-Based Anziché Polling** (Impatto: Variabile, richiede hardware)

**Descrizione**: Usare interrupt GPIO al posto del polling periodico per svegliare da deep sleep.

**Implementazione**:
```cpp
// Configura interrupt su comando MQTT (GPIO esterno)
void wake_up_callback() {
    Serial.println("[LOW-POWER] Risveglio da interrupt!");
}

attachInterrupt(INTERRUPT_PIN, wake_up_callback, RISING);

// Deep sleep con timeout + interrupt
esp_sleep_enable_ext0_wakeup(INTERRUPT_PIN, 1);
esp_sleep_enable_timer_wakeup(300 * 1000000);  // 5 minuti timeout
esp_deep_sleep_start();
```

**Vantaggi**:
- Reattività immediata a comando di disattivazione
- Consuma energia quasi zero durante sleep

**Svantaggi**:
- Richiede GPIO esterno/pulsante fisico
- Complessa integrazione con MQTT
- Necessita bateria di backup per RTC

---

### 5️⃣ **Monitoring Periodico con Deep Sleep Stack** (Impatto: 80-90% riduzione)

**Descrizione**: Usare sleep profondo con RTC wake-up anziché polling continuo.

**Implementazione**:
```cpp
// Numero cicli tra letture
static int sleepCycles = 0;

if (low && !isReadCycle) {
    sleepCycles++;
    
    if (sleepCycles >= 5) {  // 5 cicli = tempo per prossima lettura
        // Configura RTC per risveglio dopo 5 minuti
        // (Se RTC supporta interrupt)
        
        WiFi.disconnect();
        setCpuFrequencyMhz(10);
        
        // Deep sleep per 4 minuti, 50 secondi
        esp_sleep_enable_timer_wakeup(290 * 1000000);  // 290 secondi
        esp_deep_sleep_start();
        
        sleepCycles = 0;
    }
}
```

**Vantaggi**:
- Consumo minimo possibile (~1mA in deep sleep)
- Risparmio di batteria massimo per applicazioni offline

**Svantaggi**:
- Nessuna reattività a comandi durante sleep
- Richiede RTC per tracciare tempo durante deep sleep
- Complessità di sincronizzazione al risveglio

---

### 6️⃣ **Batteria Voltage Monitoring** (Impatto: Diagnostica)

**Descrizione**: Leggere tensione batteria ogni ciclo per tracciare stato di carica.

**Implementazione**:
```cpp
#define BATTERY_PIN 3       // GPIO per lettura batteria
#define BATTERY_SCALE 2.0   // Divisore tensione per lettura fino a ~8.4V

float get_battery_voltage() {
    int adcValue = analogRead(BATTERY_PIN);
    float voltage = (adcValue / 4095.0) * 3.3 * BATTERY_SCALE;
    return voltage;
}

// Nel ciclo di monitoraggio
float batteryVoltage = get_battery_voltage();
checkSensor["battery_v"] = batteryVoltage;

// Alert se tensione troppo bassa
if (batteryVoltage < 3.0) {  // ~0% per Li-Ion, ~10% per Alcaline
    Serial.printf("[ALERT] ⚠️  Batteria bassa: %.2fV\n", batteryVoltage);
    // Opzionalmente: abilita modalità ultra-low
}
```

**Vantaggi**:
- Traccia autonomia batteria in tempo reale
- Previene shutdown improvviso
- Utile per diagnostica di campo

**Svantaggi**:
- Richiede divisore resistivo su GPIO ADC
- Consume ~ 1mA quando GPIO attivo

---

### 7️⃣ **Sensori Selettivi in Low Power** (Implementazione consigliata)

**Descrizione**: Leggere solo sensori essenziali durante cicli skip, mantenere ultimi valori.

**Stato**: ✅ **GIÀ IMPLEMENTATO**

Sensori che possono essere **skippati** in cicli non-lettura:
- ❌ Ozone (O3) - consumo alto, cambamenti lenti
- ❌ Multigas (CO, NO2, etc) - consumo alto
- ❌ SCD30 - consumo moderato, non critico
- ❌ SCD41 - consumo moderato, non critico
- ❌ SEN55 - consumo alto, non critico
- ❌ SPS30 - consumo alto, calibrazione periodica

Sensori che **restano attivi** in cicli skip:
- ✅ Temperature/Humidity (DHT, BMP) - consumo bassimo
- ✅ Pressure (BMP) - consumo bassimo
- ✅ PM2.5/PM10 (PMSA003) - se abilitato e disponibile

---

## 📊 Confronto Consumo Energetico

| Modalità | Descrizione | Consumo Medio | Autonomia (800mAh) | Benefit |
|----------|-------------|--------------|-------------------|---------|
| **Normal** | Continuous monitoring | ~175mA | ~4.5 ore | — |
| **Low Power v1** | 5min read / 10min send | ~122mA | ~6.5 ore | +45% |
| **+ WiFi Off** | + WiFi spento tra cicli | ~85mA | ~9.4 ore | +110% |
| **+ Sensor Power** | + Alimentazione sensori off | ~35mA | ~22 ore | +390% |
| **+ Deep Sleep** | + Sleep profondo RTC | ~5mA | ~160 ore | +3,500% |

---

## 🎯 Raccomandazioni per Implementazione

### **Priorità ALTA** (Implementare subito):
1. ✅ Sensori skippati in cicli non-lettura (GIÀ FATTO)
2. ✅ MQTT/WiFi listen solo in cicli send (GIÀ FATTO)
3. ⏳ Batteria voltage monitoring (semplice, utile)

### **Priorità MEDIA** (Valutare):
4. WiFi radio off tra cicli
5. CPU frequency scaling più aggressivo

### **Priorità BASSA** (Advanced):
6. Controllo alimentazione sensori (richiede hardware)
7. Sleep interrupt-based (richiede hardware, complesso)
8. Deep sleep stack con RTC (massimi risparmi)

---

## 🔧 Testing & Validation

Per validare l'ottimizzazione energetica:

```bash
# 1. Misurare consumo corrente con amperometro
#    Atteso: ~122mA medio in cicli alterati

# 2. Controllare output seriale
#    [LOW-POWER] Ciclo skip sensori
#    [LOW-POWER] Ciclo skip MQTT listen

# 3. Verificare frequenza CPU
#    setCpuFrequencyMhz(10) durante skip
#    setCpuFrequencyMhz(80) durante lettura

# 4. Monitorare RTC per validare timing
#    Letture ogni 5 minuti
#    Invii ogni 10 minuti

# 5. Autonomia batteria
#    Prima: 4.5 ore
#    Dopo: 6.5+ ore (target)
```

---

## 📝 Note Importanti

- **Cicli di skip** mantengono gli ultimi dati validi dei sensori
- **MQTT rimane** attivo nei cicli di SEND per ricevere comando disattivazione
- **CPU frequency** controlla da 10MHz (sleep) a 240MHz (comunicazione)
- **Implementazione progressiva** consigliata per validare stabilità
- **Monitoraggio RTC** critico per sincronizzazione timing

---

## 🎓 Prossimi Passi

1. **Aggiungere batteria voltage monitoring** (impatto alto, complessità bassa)
2. **Testare autonomia** con misuratore corrente
3. **Considerare WiFi off** se autonomia non soddisfa
4. **Valutare sensori power control** per deployment campo

