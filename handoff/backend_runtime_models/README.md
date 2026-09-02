# Runtime models Sensy — handoff backend

Questo pacchetto aggiunge al backend Flask:

- route principale `GET /get_runtime_models?ID=...`;
- restituzione di tutte le equazioni correnti della Sensy in una sola risposta;
- storico locale firmware resistente ai blackout; bootstrap MongoDB resta nella route singola legacy;
- import diretto dal manifest prodotto da `Calibration_Model`;
- import alternativo da equazione testuale;
- sovrascrittura atomica del modello corrente in MongoDB.

## File

- `server_centraline_runtime_model.py`: route Flask e accesso MongoDB.
- `runtime_model_manifest_importer.py`: import preferito dal manifest JSON di modelling.
- `runtime_equation_parser.py`: fallback per equazioni disponibili solo come stringa.
- `examples/runtime_equation_input.txt`: equazione testuale di esempio.
- `examples/runtime_model_response_v1.json`: risposta JSON di esempio.
- `examples/runtime_models_response_v1.json`: risposta bulk usata dal firmware.
- `self_test.py`: test rapido senza accesso a Flask o MongoDB.

## Dipendenze

```bash
python -m pip install -r requirements.txt
```

## Integrazione nell'app Flask esistente

Copia i tre moduli Python accanto ad `app.py`. Dopo aver creato `app` e la connessione a MongoDB:

```python
from server_centraline_runtime_model import register_runtime_model_route

SSDB = mongo_client["SSDB"]
register_runtime_model_route(app, SSDB)
```

`SSDB` deve essere un oggetto `pymongo.database.Database`, non un `MongoClient`.

Creare gli indici una volta durante un deployment controllato:

```python
from server_centraline_runtime_model import ensure_runtime_model_indexes

ensure_runtime_model_indexes(SSDB)
```

La collection proposta è `SSDB.sensy_runtime_models`. Il nome può essere cambiato tramite la costante `MODEL_COLLECTION`.

L'indice unico `(ID, pollutant)` garantisce un solo modello corrente per coppia. Ogni nuovo caricamento usa `replace_one(..., upsert=True)` e sovrascrive il documento precedente; non viene mantenuto uno storico delle equazioni.

## Richiesta Sensy

La Sensy invia un solo parametro:

```http
GET /get_runtime_models?ID=ITCURCRDIKXHW1
```

Risposta: oggetto root autorevole. Ogni chiave è un inquinante (`no2`, `co`, `c6h6`, ecc.); valore è relativa equazione. Nessun wrapper aggiuntivo.

Mapping Multigas: chiave/output `c6h6` usa come ingresso `inputs.multigas_voc_raw` con source `GM502B`. C6H6 non ha un canale raw separato; non rinominare output in `voc`.

Risposte:

- `200`: snapshot valido; `{}` indica nessuna equazione configurata;
- `400`: `ID` non valido;
- `500`: documento modello non conforme;
- `503`: errore MongoDB.

La vecchia route singola `GET /get_runtime_model?ID=...&Pollutant=...` resta disponibile per compatibilità.

## Caricamento preferito dal manifest modelling

Il manifest contiene coefficienti con precisione completa e metadati del training. Non è necessario analizzare la formula visuale.

```python
from server_centraline_runtime_model import load_runtime_model_from_manifest

model_id = load_runtime_model_from_manifest(
    SSDB,
    manifest_path="avellino_advanced_gas_models_v3_manifest.json",
    sensy_id="ITCURCRDIKXHW1",
    pollutant="NO2",
)
```

L'importer seleziona esattamente:

- stesso `device` e `pollutant`;
- `sensor_family = Multigas`;
- `mode = Standalone sensor-only`;
- `selection = Best deployable equation`;
- `prediction_transform = direct`.

I modelli `Rejected` vengono bloccati. Un caricamento deliberato richiede `allow_rejected=True`. Le feature non supportate dal firmware vengono bloccate e indicate nell'errore; non vengono mai ignorate.

Configurazione del manifest Avellino:

- aggregazione input: oraria;
- `t-1h`: precedente media oraria;
- `t-2h`: seconda media oraria precedente;
- `rolling3h`: media delle ultime tre medie orarie;
- origine `Elapsed days`: `2026-08-05 08:00:00 UTC`, epoch `1785916800`.

## Caricamento alternativo da stringa

```python
from server_centraline_runtime_model import load_runtime_equation

model_id = load_runtime_equation(
    SSDB,
    sensy_id="ITCURCRDIKXHW1",
    pollutant="NO2",
    equation=equation_string,
    output_unit="ug/m3",
    elapsed_days_origin_epoch=1785916800,
    no2_raw_scale=1.0,
    voc_raw_scale=1.0,
    sample_period_seconds=3600,
)
```

Non usare `eval()` sulle equazioni. Il parser incluso riconosce solo il vocabolario di feature previsto e rifiuta termini sconosciuti, duplicati o mancanti.

## Storico `centraline_minute_avg`

Prima del deployment verificare un documento reale e correggere, se necessario, il dizionario `HISTORY_FIELDS` in `server_centraline_runtime_model.py`.

Valori attualmente assunti:

```python
HISTORY_FIELDS = {
    "id": "ID",
    "timestamp": "timestamp",
    "timestamp_is_datetime": False,
    "no2_raw_candidates": ("multigas_no2_raw", "Multigas NO2 [raw]"),
    "voc_raw_candidates": ("multigas_voc_raw", "Multigas VOC [raw]"),
}
```

Se `timestamp` è un BSON Date, impostare `timestamp_is_datetime = True`.

## Sicurezza operativa

Le funzioni di caricamento sono funzioni amministrative. Non esporle come route pubblica senza autenticazione e autorizzazione. La route destinata alla Sensy è soltanto in lettura.

## Verifica rapida

```bash
python self_test.py
python -m py_compile runtime_equation_parser.py runtime_model_manifest_importer.py server_centraline_runtime_model.py
```

Output atteso:

```text
self_test=OK
```

## Limite firmware attuale

Firmware conserva e applica più modelli, uno per inquinante. Il contratto delle feature di input supporta però solo `Multigas NO2` e `Multigas VOC`. I modelli che richiedono `Multigas CO [raw]`, presenti per alcuni dispositivi ITDR, vengono bloccati finché il firmware non implementa quella feature.
