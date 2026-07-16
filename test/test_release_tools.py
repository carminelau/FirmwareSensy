"""Unit tests for release naming and safe OTA compatibility selection."""

import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / filename)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader
    spec.loader.exec_module(module)
    return module


build_matrix = load("build_matrix", "build_matrix.py")
ota_release = load("ota_release", "ota_release.py")


class ReleaseToolTests(unittest.TestCase):
    def test_release_filename_matches_eeprom_contract(self):
        self.assertEqual(
            build_matrix.release_filename("sensy_2024_V4_black", "beta", "V5_4"),
            "BT_2024V4_BK_V5_4.bin",
        )

    def test_release_filename_rejects_storage_overflow(self):
        with self.assertRaises(ValueError):
            build_matrix.release_filename("sensy_2024_V2_ENEA", "stable", "V123_456")

    def test_branch_release_increments_latest_local_artifact_version(self):
        with tempfile.TemporaryDirectory() as directory:
            artifacts = Path(directory)
            (artifacts / "BT_2024V4_BK_V5_4.bin").touch()
            (artifacts / "ST_2024V4_BK_V5_10.bin").touch()
            self.assertEqual(build_matrix.latest_release_version(artifacts), "V5_10")
        self.assertEqual(build_matrix.increment_version("V5_10"), "V5_11")

    def test_all_board_codes_fit_constant_ota_name_size(self):
        filenames = [build_matrix.release_filename(environment, "stable", "V5_4") for environment in build_matrix.ENVIRONMENTS]
        self.assertTrue(all(len(filename) == 21 for filename in filenames))

    def test_get_id_board_matches_firmware_parameter(self):
        self.assertEqual(ota_release.get_id_board("sensy_2024_V4_black"), "2024V4_BK")
        self.assertEqual(ota_release.get_id_board("2024V2_EN"), "2024V2_EN")

    def test_firmware_name_requires_exact_contract(self):
        self.assertEqual(ota_release.firmware_name("BT_2024V4_BK_V5_4.bin"), "BT_2024V4_BK_V5_4.bin")
        with self.assertRaises(ValueError):
            ota_release.firmware_name("BT_2024V4_BK_V5_40.bin")

    def test_stable_firmware_becomes_default_only_for_its_board(self):
        stable = ota_release.board_firmware_payload("ST_2024V4_BK_V5_4.bin")
        beta = ota_release.board_firmware_payload("BT_2024V4_BK_V5_4.bin")
        self.assertEqual(stable["versione"], "ST_2024V4_BK_V5_4.bin")
        self.assertEqual(stable["suitable_board"], '["2024V4_BK"]')
        self.assertEqual(stable["default_board"], '["2024V4_BK"]')
        self.assertEqual(beta["default_board"], "[]")

    def test_upload_expands_windows_shell_independent_glob(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ST_2024V4_BK_V5_4.bin").touch()
            paths = ota_release.firmware_paths([str(root / "*.bin")])
        self.assertEqual([path.name for path in paths], ["ST_2024V4_BK_V5_4.bin"])

    def test_select_centraline_accepts_id_or_alias_and_reports_unknown(self):
        selected, missing = ota_release.select_centraline(
            [{"ID": "SENSY001", "alias": "Casa"}, {"ID": "SENSY002", "alias": "Ufficio"}],
            ["casa", "SENSY002", "mancante"],
        )
        self.assertEqual([item["ID"] for item in selected], ["SENSY001", "SENSY002"])
        self.assertEqual(missing, ["mancante"])

    def test_device_ids_accepts_plain_id_array_from_elenco_centraline(self):
        centraline = ota_release.device_ids({"result": ["SENSY001", "SENSY002"]})
        self.assertEqual(centraline, [{"ID": "SENSY001", "alias": ""}, {"ID": "SENSY002", "alias": ""}])

    def test_plan_explicitly_requests_all_admin_accessible_centraline(self):
        class RecordingApi:
            def __init__(self):
                self.fields = None

            def post(self, route, fields=None):
                if route == "elenco_centraline":
                    self.fields = fields
                    return {"result": []}
                raise AssertionError(f"route inattesa: {route}")

        api = RecordingApi()
        ota_release.make_plan(api, "BT_2024V4_BK_V5_4.bin")
        self.assertEqual(api.fields, {"personal": "False"})

    def test_selected_centraline_uses_query_and_inferrs_board_from_firmware(self):
        class QueryApi:
            def __init__(self):
                self.queries = []

            def post(self, route, fields=None):
                if route == "elenco_centraline":
                    self.queries.append(fields.get("query"))
                    return {"result": [fields["query"]]}
                return {"result": {"board": "Undefined", "firmware": "ST_2024V4_BK_V4_2.bin"}}

        api = QueryApi()
        plan = ota_release.make_plan(api, "BT_2024V4_BK_V5_5.bin", ["SENSY001"])
        self.assertEqual(api.queries, ["SENSY001"])
        self.assertEqual(plan["not_found_centraline"], [])
        self.assertEqual(plan["devices"][0]["status"], "needs_update")
        self.assertEqual(plan["devices"][0]["board_source"], "firmware")

    def test_force_only_allows_unknown_explicitly_selected_board(self):
        class QueryApi:
            def post(self, route, fields=None):
                if route == "elenco_centraline":
                    return {"result": [fields["query"]]}
                return {"result": {"board": "Undefined", "firmware": "legacy.bin"}}

        plan = ota_release.make_plan(QueryApi(), "BT_2024V4_BK_V5_5.bin", ["SENSY001"], force_unknown_board=True)
        self.assertTrue(plan["devices"][0]["forced"])
        self.assertEqual(plan["devices"][0]["status"], "needs_update")

    def test_plan_selects_every_device_with_matching_get_id_board(self):
        class FakeApi:
            def post(self, route, fields=None):
                if route == "elenco_centraline":
                    return {"result": [{"ID": "black-old"}, {"ID": "black-new"}, {"ID": "green-old"}]}
                records = {
                    "black-old": {"result": {"board": "sensy_2024_V4_black", "firmware": "ST_2024V4_BK_V5_3.bin"}},
                    "black-new": {"result": {"board": "sensy_2024_V4_black", "firmware": "BT_2024V4_BK_V5_4.bin"}},
                    "green-old": {"result": {"board": "sensy_2024_V4_green", "firmware": "BT_2024V4_GR_V5_3.bin"}},
                }
                return records[fields["ID"]]

        plan = ota_release.make_plan(FakeApi(), "BT_2024V4_BK_V5_4.bin")
        statuses = {item["ID"]: item["status"] for item in plan["devices"]}
        self.assertEqual(statuses, {"black-old": "needs_update", "black-new": "up_to_date", "green-old": "unknown"})

    def test_platformio_ota_board_matches_release_board_code(self):
        config = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        for environment, board in build_matrix.RELEASE_BOARD_CODES.items():
            section = config.split(f"[env:{environment}]", 1)[1].split("[env:", 1)[0]
            self.assertIn(f'-DOTA_BOARD=\\"{board}\\"', section)


if __name__ == "__main__":
    unittest.main()
