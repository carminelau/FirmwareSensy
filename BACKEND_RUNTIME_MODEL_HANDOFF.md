# Handoff backend: modelli runtime Sensy

## Server e route

Server: `server_centraline`, file esistente `server_centraline/app.py`.

```http
GET https://square.sensesquare.eu:5000/get_runtime_models?ID=SENSY_ID
```

Firmware attuale usa host `sensy.sensesquare.eu:5000`. Verificare che sia alias dello stesso `server_centraline`; altrimenti aggiornare host firmware o DNS.

Unico parametro: `ID` Sensy. La route restituisce tutte le equazioni correnti associate a tale ID.

Esempio:

```bash
curl -i "https://square.sensesquare.eu:5000/get_runtime_models?ID=SENSY_ID"
```

## Integrazione Flask

Copia `samples/server_centraline_runtime_model.py` nel progetto backend e registra route dopo creazione di `app` e connessione a `SSDB`:

```python
from server_centraline_runtime_model import register_runtime_model_route

register_runtime_model_route(app, SSDB)
```

`SSDB` deve essere oggetto `pymongo.database.Database`, non `MongoClient`.

## Caricamento da equazione testuale

Metodo preferito quando modello proviene dalla pipeline `Calibration_Model`: import diretto dal manifest JSON. Coefficienti strutturati mantengono precisione completa e non richiedono parsing della stringa.

```python
from server_centraline_runtime_model import load_runtime_model_from_manifest

model_id = load_runtime_model_from_manifest(
    SSDB,
    manifest_path="avellino_advanced_gas_models_v3_manifest.json",
    sensy_id="ITCURCRDIKXHW1",
    pollutant="NO2",
)
```

Importer seleziona solo:

- stesso `device` e `pollutant`;
- `sensor_family = Multigas`;
- `mode = Standalone sensor-only`;
- `selection = Best deployable equation`;
- `prediction_transform = direct`.

Modelli con stato `Rejected` vengono bloccati. Override richiede `allow_rejected=True`. Feature non supportate dal firmware, per esempio `Multigas CO [raw]`, producono errore invece di essere ignorate.

Configurazione nota per manifest Avellino: input orari e origine `Elapsed days` pari a `2026-08-05 08:00:00 UTC` (`1785916800`).

Formato testuale resta fallback per equazioni ricevute fuori dalla pipeline.

Equazione può restare nel formato umano mostrato in `samples/runtime_equation_input.txt`. Non usare `eval()`.

Funzione `load_runtime_equation()` esegue conversione, validazione e sovrascrittura della coppia `(ID, pollutant)`:

```python
from server_centraline_runtime_model import load_runtime_equation

model_id = load_runtime_equation(
    SSDB,
    sensy_id="SENSY_ID",
    pollutant="no2",
    equation=equation_string,
    output_unit="ug/m3",
    elapsed_days_origin_epoch=1704067200,
    no2_raw_scale=1.0,
    voc_raw_scale=1.0,
    sample_period_seconds=300,
)
```

Parser accetta solo termini noti del contratto e rifiuta termini mancanti, duplicati o sconosciuti. Quattro informazioni non esistono nella stringa e devono arrivare dal training/configurazione backend:

- unità output;
- epoch usato per calcolare `Elapsed days`;
- scala dei due raw;
- cadenza campioni.

`model_id` restituito è hash automatico. Per caricare molte coppie, chiamare stessa funzione per ogni `ID` e `pollutant`. Non serve route pubblica di scrittura: chiamarla da script amministrativo, job protetto o pipeline training.

## Selezione equazione

Collection proposta: `SSDB.sensy_runtime_models` (il nome non è obbligatorio).

Contiene solo la configurazione corrente: una sola entry per coppia `(ID, pollutant)`, senza storico delle equazioni.

Per `pollutant = c6h6`, modello deve usare `Multigas VOC [raw]`, mappato a `inputs.multigas_voc_raw` con source `GM502B`. `output.field` resta `c6h6`; non usare `voc` come chiave output.

```json
{
  "ID": "SENSY_ID",
  "pollutant": "no2",
  "schema_version": 1,
  "enabled": true,
  "model_id": "sha256-6f42c431df21a892",
  "refresh_after_seconds": 21600,
  "output": {},
  "inputs": {},
  "history": {},
  "temperature_threshold_c": 30.0,
  "coefficients": {}
}
```

Indice unico richiesto su `(ID, pollutant)`. Funzione `ensure_runtime_model_indexes(SSDB)` lo crea insieme all'indice storico `(ID, timestamp)`. Eseguirla una volta durante deployment controllato.

`upsert_runtime_model()` usa `replace_one(..., upsert=True)`: se la coppia esiste, il documento precedente viene sovrascritto; altrimenti viene creato. Non rimangono vecchie equazioni.

Il backend calcola automaticamente `model_id` come hash del contenuto corrente. Non è una versione e non crea storico: permette alla Sensy di riconoscere un'equazione cambiata dopo aver ricevuto la risposta.

## Bootstrap storico

La route bulk restituisce soltanto equazioni. Lo storico viene mantenuto localmente dalla Sensy. La vecchia route singola conserva `build_history_bootstrap()` per compatibilità con client precedenti.

Campi MongoDB assunti:

- `ID`;
- `timestamp` come epoch numerico;
- `multigas_no2_raw` oppure `Multigas NO2 [raw]`;
- `multigas_voc_raw` oppure `Multigas VOC [raw]`.

Se `timestamp` è BSON Date, impostare `HISTORY_FIELDS["timestamp_is_datetime"] = True`.

Prima del merge, verificare nomi campi su un documento reale di `centraline_minute_avg`. Modifiche restano isolate nel dizionario `HISTORY_FIELDS`.

## Risposte

- `200`: oggetto root autorevole; ogni chiave è un inquinante e `{}` indica nessuna equazione;
- `400`: parametri errati;
- `500`: modello MongoDB non conforme;
- `503`: database non disponibile.

`GET /get_runtime_model?ID=...&Pollutant=...` resta disponibile solo per compatibilità con client precedenti.

## Lato firmware

Firmware esegue una sola richiesta con `ID`, valida atomicamente tutti i modelli e conserva ultimo snapshot valido. Modelli multipli vengono calcolati indipendentemente e inseriti nello stesso payload Sensy. Modelli e storico usano due slot SPIFFS con checksum, così uno spegnimento durante la scrittura non distrugge ultima copia valida. Dopo un blackout lungo, campioni mancanti non vengono inventati: output calibrato riparte quando lag e rolling dispongono nuovamente di misure reali.
