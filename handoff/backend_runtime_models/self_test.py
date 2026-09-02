import json
from pathlib import Path

from runtime_equation_parser import build_runtime_model_payload
from runtime_model_manifest_importer import manifest_model_to_runtime_payload


root = Path(__file__).resolve().parent
equation = (root / "examples" / "runtime_equation_input.txt").read_text(
    encoding="utf-8"
)

text_payload = build_runtime_model_payload(
    equation,
    output_field="no2",
    output_unit="ug/m3",
    elapsed_days_origin_epoch=1785916800,
    no2_raw_scale=1.0,
    voc_raw_scale=1.0,
    sample_period_seconds=3600,
)
assert len(text_payload["coefficients"]) == 19
assert text_payload["history"]["minimum_rolling_samples"] == 3

manifest_payload = manifest_model_to_runtime_payload(
    {
        "device": "SENSY_TEST",
        "pollutant": "NO2",
        "sensor_family": "Multigas",
        "mode": "Standalone sensor-only",
        "selection": "Best deployable equation",
        "status": "Exploratory",
        "algorithm": "Huber linear",
        "prediction_transform": "direct",
        "intercept": 10.0,
        "coefficients": {
            "Multigas NO2 [raw]": 2.0,
            "Temperature [°C]": -0.5,
        },
        "equation": "y = 10 + 2×Multigas NO2 [raw] - 0.5×Temperature [°C]",
    }
)
assert manifest_payload["output"]["field"] == "no2"
assert manifest_payload["coefficients"]["no2_raw"] == 2.0
assert manifest_payload["coefficients"]["no2_raw_squared"] == 0.0

bulk_payload = json.loads(
    (root / "examples" / "runtime_models_response_v1.json").read_text(encoding="utf-8")
)
assert list(bulk_payload) == ["no2", "c6h6"]
assert bulk_payload["no2"]["output"]["field"] == "no2"
assert bulk_payload["c6h6"]["output"]["field"] == "c6h6"
assert bulk_payload["c6h6"]["inputs"]["multigas_voc_raw"]["source"] == "GM502B"

print("self_test=OK")
