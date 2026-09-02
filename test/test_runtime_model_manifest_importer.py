import pathlib
import sys
import unittest


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "samples"))

from runtime_model_manifest_importer import (
    DEFAULT_TRAINING_ORIGIN_EPOCH,
    manifest_model_to_runtime_payload,
    select_manifest_model,
)


def model_fixture(**overrides):
    model = {
        "device": "SENSY_1",
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
    model.update(overrides)
    return model


class RuntimeModelManifestImporterTests(unittest.TestCase):
    def test_selects_exact_deployable_pair(self):
        selected = select_manifest_model(
            {"models": [model_fixture()]}, "SENSY_1", "no2"
        )
        self.assertEqual("NO2", selected["pollutant"])

    def test_linear_model_fills_unused_coefficients_with_zero(self):
        payload = manifest_model_to_runtime_payload(model_fixture())

        self.assertEqual("no2", payload["output"]["field"])
        self.assertEqual(3600, payload["history"]["sample_period_seconds"])
        self.assertEqual(7200, payload["history"]["rolling_window_seconds"])
        self.assertEqual(900, payload["history"]["lag_tolerance_seconds"])
        self.assertEqual(3, payload["history"]["minimum_rolling_samples"])
        self.assertEqual(
            DEFAULT_TRAINING_ORIGIN_EPOCH,
            payload["inputs"]["elapsed_days_origin_epoch"],
        )
        self.assertEqual(2.0, payload["coefficients"]["no2_raw"])
        self.assertEqual(0.0, payload["coefficients"]["no2_raw_squared"])

    def test_rejected_model_requires_explicit_override(self):
        with self.assertRaisesRegex(ValueError, "Rejected"):
            manifest_model_to_runtime_payload(model_fixture(status="Rejected"))

    def test_rejects_feature_not_supported_by_firmware(self):
        model = model_fixture(
            coefficients={"Multigas CO [raw]": 1.0},
        )
        with self.assertRaisesRegex(ValueError, "Multigas CO"):
            manifest_model_to_runtime_payload(model)


if __name__ == "__main__":
    unittest.main()
