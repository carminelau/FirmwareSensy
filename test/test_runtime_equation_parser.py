import json
import pathlib
import sys
import unittest


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "samples"))

from runtime_equation_parser import build_runtime_model_payload, parse_equation_string


class RuntimeEquationParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.equation = (REPOSITORY_ROOT / "samples" / "runtime_equation_input.txt").read_text(
            encoding="utf-8"
        )

    def test_parses_reference_equation(self):
        parsed = parse_equation_string(self.equation)

        self.assertEqual(19, len(parsed["coefficients"]))
        self.assertEqual(30.0, parsed["temperature_threshold_c"])
        self.assertAlmostEqual(89.84894046, parsed["coefficients"]["intercept"])
        self.assertAlmostEqual(-0.00771123405, parsed["coefficients"]["humidity"])
        self.assertAlmostEqual(
            -2.511363471,
            parsed["coefficients"]["temperature_above_threshold"],
        )

    def test_builds_firmware_payload(self):
        payload = build_runtime_model_payload(
            self.equation,
            output_field="no2",
            output_unit="ug/m3",
            elapsed_days_origin_epoch=1704067200,
            no2_raw_scale=1.0,
            voc_raw_scale=1.0,
        )

        self.assertEqual(1, payload["schema_version"])
        self.assertEqual("no2", payload["output"]["field"])
        self.assertEqual(36, payload["history"]["minimum_rolling_samples"])

    def test_rejects_unknown_or_missing_terms(self):
        with self.assertRaises(ValueError):
            parse_equation_string("y = 1 + 2×Unknown input")

    def test_bulk_response_sample_is_keyed_directly_by_pollutant(self):
        payload = json.loads(
            (REPOSITORY_ROOT / "samples" / "runtime_models_response_v1.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(["no2", "c6h6"], list(payload))
        self.assertEqual("no2", payload["no2"]["output"]["field"])
        self.assertEqual("c6h6", payload["c6h6"]["output"]["field"])
        self.assertEqual("GM502B", payload["c6h6"]["inputs"]["multigas_voc_raw"]["source"])
        self.assertEqual(1.0, payload["c6h6"]["coefficients"]["voc_raw"])

    def test_device_demo_equation_is_easy_to_verify(self):
        payload = json.loads(
            (
                REPOSITORY_ROOT
                / "samples"
                / "demo_calibration_equations_ITPHVQWGHETJL3.json"
            ).read_text(encoding="utf-8")
        )
        coefficients = payload["no2"]["coefficients"]
        self.assertEqual(10.0, coefficients["intercept"])
        self.assertEqual(2.0, coefficients["no2_raw"])
        self.assertTrue(
            all(
                value == 0.0
                for name, value in coefficients.items()
                if name not in {"intercept", "no2_raw"}
            )
        )


if __name__ == "__main__":
    unittest.main()
