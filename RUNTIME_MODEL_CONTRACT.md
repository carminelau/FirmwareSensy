# Runtime model HTTP contract v1

## Scopo

Firmware scarica coefficienti dal backend e calcola localmente la previsione. Non interpreta testo matematico libero: usa JSON versionato, validabile e più sicuro su ESP32.

Sample risposta bulk: `samples/runtime_models_response_v1.json`. Il file `runtime_model_response_v1.json` mostra il valore associato a una singola chiave inquinante.

## Request firmware

```http
GET /get_runtime_models?ID=DEVICE_ID HTTP/1.1
Host: sensy.sensesquare.eu:5000
```

Sensy invia solo `ID`. Backend restituisce lo snapshot autorevole di tutte le equazioni correnti:

- `200 application/json`: oggetto root dove ogni chiave è un inquinante e ogni valore è il relativo modello;
- altri status: firmware conserva ultimo modello valido in cache.

Dimensione massima risposta: 65536 byte. Ogni modello conserva il proprio `schema_version: 1`.

Firmware valida tutto lo snapshot prima di attivarlo. Ogni chiave root è un inquinante, per esempio `no2`, e `output.field` deve coincidere con tale chiave. Snapshot e storico sono salvati in due slot SPIFFS alternati, con sequenza e checksum. Cache snapshot è associata internamente all'hash dell'ID Sensy, evitando uso dopo cambio ID.

Esempio abbreviato; ogni valore contiene il modello completo mostrato nel sample:

```json
{
  "no2": {"schema_version": 1, "enabled": true, "output": {"field": "no2"}},
  "c6h6": {"schema_version": 1, "enabled": true, "output": {"field": "c6h6"}}
}
```

Target runtime ammessi: `c2h5oh`, `c6h6`, `co`, `co2`, `nh3`, `no2`, `nox_index`, `o3`, `pm1`, `pm10`, `pm2_5`, `so2`, `voc`, `voc_index`. Firmware registra soltanto quelli associati ai sensori rilevati; con Multigas aggiunge anche `c6h6`.

Mapping obbligatorio Multigas: `c6h6` è output benzene calibrato usando canale raw VOC `GM502B` (`inputs.multigas_voc_raw`). Non esiste un raw C6H6 separato. Chiave JSON e `output.field` restano `c6h6`; solo input hardware è VOC.

## Dati forniti dal backend

- Identità/versione modello: `model_id`, `schema_version`, `enabled`.
- Destinazione: `output.field`, unità e clamp opzionali. `null` conserva equazione senza clamp.
- Scala dei raw: `scale` e `offset` per GM102B/GM502B.
- Origine di `Elapsed days`: epoch UTC identico a quello usato nel training.
- Semantica temporale: periodo campioni, lag, finestra rolling, tolleranza e copertura minima.
- Soglia hinge temperatura.
- Intercetta e tutti coefficienti.
- Frequenza aggiornamento: `refresh_after_seconds`, tra 300 e 604800 secondi.

Backend non deve inviare valori sensore correnti, lag calcolati o termini polinomiali.

## Dati calcolati a runtime da Sensy

- Letture hardware GM102B e GM502B correnti.
- `temperatura` e `umidita` finali già selezionate dal normale payload Sensy.
- Campioni raw storici, lag t-1h/t-2h e medie rolling 3h.
- Giorni trascorsi dall'epoch fornito dal backend.
- Quadrati, interazioni e `max(0, temperatura - soglia)`.
- Un valore finale per ogni modello abilitato, pubblicato nel relativo `output.field`.

Storia raw usa 64 campioni con timestamp, condivisi tra modelli e conservati sia in RTC memory sia in SPIFFS. Sopravvive quindi anche a perdita completa di alimentazione. Firmware usa la cadenza più fitta richiesta dai modelli caricati. Se `require_full_rolling_window` è `true`, ogni modello attende indipendentemente lag e finestra completa.

Uno spegnimento non contiene misure. Se dura più della finestra richiesta, firmware non interpola e non riutilizza campioni vecchi come se fossero recenti: sospende il valore calibrato finché lag e rolling sono nuovamente completi. Risposta bulk contiene soltanto equazioni; storico resta responsabilità locale della Sensy.

## Decisioni da confermare col training

1. `scale: 0.01` replica conversione firmware `getGMxxxxB() / 100.0`. Se modello è stato addestrato sul valore intero restituito dalla libreria, usare `scale: 1.0`.
2. Manifest Avellino usa `elapsed_days_origin_epoch: 1785916800` (`2026-08-05 08:00 UTC`).
3. `output.field` deve essere uguale alla chiave inquinante root, normalizzata in minuscolo.
4. Decidere eventuale `clamp_min: 0.0`; sample usa `null` per riprodurre equazione esatta.
5. Confermare che `published_temperatura` e `published_umidita` corrispondano alle colonne usate nel training.
6. Modelli Avellino usano medie orarie: `sample_period_seconds: 3600`, tre campioni per rolling e span tra primo e ultimo centro orario pari a `7200` secondi.

## Mapping formula - JSON

- `[t-1h]`: coefficienti `*_lag_1`, con `lag_1_seconds: 3600`.
- `[t-2h]`: coefficienti `*_lag_2`, con `lag_2_seconds: 7200`.
- `[rolling3h]`: coefficienti `*_rolling`; per tre medie orarie inclusive, `rolling_window_seconds: 7200` e `minimum_rolling_samples: 3`.
- `max(0, Temperature - 30)`: `temperature_above_threshold` e `temperature_threshold_c: 30.0`.
