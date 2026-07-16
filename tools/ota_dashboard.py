#!/usr/bin/env python3
"""Local Sense Square OTA dashboard. Open http://127.0.0.1:8765 after launch."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import uuid
import webbrowser
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parents[1]
WEB_ROOT = ROOT / "tools" / "ota_dashboard_web"
CONFIG_PATH = Path(os.environ.get("APPDATA", str(Path.home()))) / "SenseSquare" / "ota_dashboard.json"
sys.path.insert(0, str(ROOT / "scripts"))
import ota_release as ota  # noqa: E402

PLANS: dict[str, dict] = {}
DASHBOARD_API_KEY: str | None = None


def load_config() -> dict:
    try:
        value = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (FileNotFoundError, OSError, json.JSONDecodeError):
        return {}


def project_root() -> Path:
    config = load_config()
    configured = config.get("project_root")
    if not configured and config.get("firmware_dir"):
        configured = str(Path(str(config["firmware_dir"])).parent)
    return Path(str(configured or ROOT)).expanduser().resolve()


def firmware_dir() -> Path:
    return project_root() / "dist"


def save_project_root(value: str) -> Path:
    if not value or not isinstance(value, str):
        raise ValueError("cartella progetto firmware richiesta")
    raw_directory = Path(value).expanduser()
    if not raw_directory.is_absolute():
        raise ValueError("usare percorso assoluto")
    directory = raw_directory.resolve()
    if not (directory / "platformio.ini").is_file() or not (directory / "scripts" / "build_matrix.py").is_file():
        raise ValueError("cartella non valida: servono platformio.ini e scripts/build_matrix.py")
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(json.dumps({"project_root": str(directory)}, indent=2), encoding="utf-8")
    return directory


def api_key() -> str | None:
    if DASHBOARD_API_KEY:
        return DASHBOARD_API_KEY
    value = os.environ.get(ota.API_KEY_ENV)
    if value:
        return value
    try:
        import winreg

        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Environment") as key:
            return winreg.QueryValueEx(key, ota.API_KEY_ENV)[0]
    except (ImportError, FileNotFoundError, OSError):
        return None


def releases() -> list[str]:
    directory = firmware_dir()
    if not directory.is_dir():
        return []
    return sorted((path.name for path in directory.glob("*.bin") if ota.FIRMWARE_RE.fullmatch(path.name)), reverse=True)


def release_path(name: str) -> Path:
    safe_name = ota.firmware_name(name)
    directory = firmware_dir()
    path = (directory / safe_name).resolve()
    if path.parent != directory or not path.is_file():
        raise ValueError("firmware non disponibile nel percorso configurato")
    return path


class DashboardHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_ROOT), **kwargs)

    def log_message(self, format, *args):
        return

    def send_json(self, status: int, payload: dict) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", "0"))
        if length > 1_000_000:
            raise ValueError("richiesta troppo grande")
        value = json.loads(self.rfile.read(length).decode("utf-8"))
        if not isinstance(value, dict):
            raise ValueError("JSON oggetto richiesto")
        return value

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/status":
            root = project_root()
            directory = firmware_dir()
            self.send_json(HTTPStatus.OK, {
                "api_key_configured": bool(api_key()),
                "releases": releases(),
                "project_root": str(root),
                "project_root_exists": root.is_dir(),
                "firmware_dir": str(directory),
                "firmware_dir_exists": directory.is_dir(),
                "config_path": str(CONFIG_PATH),
            })
            return
        if path == "/api/remote-firmwares":
            try:
                response = ota.SquareApi(api_key()).post("lista_firmware")
                result = response.get("result", []) if isinstance(response, dict) else []
                if not ota.response_ok(response) or not isinstance(result, list):
                    raise ValueError("catalogo firmware remoto non disponibile")
                names = sorted({item for item in result if isinstance(item, str) and ota.FIRMWARE_RE.fullmatch(item)}, reverse=True)
                self.send_json(HTTPStatus.OK, {"releases": names})
            except (ValueError, RuntimeError) as exc:
                self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
            except Exception as exc:
                self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": f"errore server: {exc}"})
            return
        if self.path == "/":
            self.path = "/index.html"
        return super().do_GET()

    def do_POST(self):
        try:
            payload = self.read_json()
            path = urlparse(self.path).path
            if path == "/api/build":
                return self.build(payload)
            if path == "/api/config":
                return self.config(payload)
            if path == "/api/upload":
                return self.upload(payload)
            if path == "/api/plan":
                return self.plan(payload)
            if path == "/api/apply":
                return self.apply(payload)
            self.send_json(HTTPStatus.NOT_FOUND, {"error": "endpoint non trovato"})
        except (ValueError, RuntimeError, json.JSONDecodeError) as exc:
            self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
        except Exception as exc:
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": f"errore server: {exc}"})

    def build(self, payload: dict) -> None:
        root = project_root()
        build_script = root / "scripts" / "build_matrix.py"
        if not build_script.is_file():
            raise ValueError("progetto firmware non valido: scripts/build_matrix.py assente")
        command = [sys.executable, str(build_script), "--check", "--artifacts-dir", str(firmware_dir())]
        if payload.get("branch_release"):
            command.append("--branch-release")
        else:
            channel = payload.get("channel")
            version = payload.get("version")
            if channel not in ota.CHANNEL_CODES or not isinstance(version, str):
                raise ValueError("selezionare canale e versione")
            command.extend(["--channel", channel, "--version", version])
        for environment in payload.get("environments", []):
            if not isinstance(environment, str) or not environment.replace("_", "").isalnum():
                raise ValueError(f"ambiente non valido: {environment}")
            command.extend(["--env", environment])
        process = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.send_json(HTTPStatus.OK if process.returncode == 0 else HTTPStatus.BAD_REQUEST, {
            "ok": process.returncode == 0,
            "output": process.stdout[-12000:],
            "releases": releases(),
        })

    def config(self, payload: dict) -> None:
        global DASHBOARD_API_KEY
        if "api_key" in payload:
            value = payload["api_key"]
            if not isinstance(value, str):
                raise ValueError("API key non valida")
            DASHBOARD_API_KEY = value.strip() or None
        if "project_root" in payload:
            root = save_project_root(payload["project_root"])
        else:
            root = project_root()
        self.send_json(HTTPStatus.OK, {
            "api_key_configured": bool(api_key()),
            "project_root": str(root),
            "firmware_dir": str(firmware_dir()),
            "releases": releases(),
        })

    def upload(self, payload: dict) -> None:
        names = payload.get("firmwares")
        if not isinstance(names, list) or not names:
            raise ValueError("selezionare almeno un firmware")
        api = ota.SquareApi(None)
        results = []
        for name in names:
            firmware = release_path(str(name))
            response = api.post(
                "set_board_firmware",
                ota.board_firmware_payload(firmware.name),
                ("contenuto", firmware),
                base_url=ota.FIRMWARE_API_BASE,
                include_api_key=False,
            )
            results.append({"firmware": firmware.name, "ok": ota.response_ok(response), "response": response})
        self.send_json(HTTPStatus.OK, {"results": results})

    def plan(self, payload: dict) -> None:
        firmware = ota.firmware_name(str(payload.get("firmware", "")))
        centraline = payload.get("centraline") or []
        if not isinstance(centraline, list) or not all(isinstance(item, str) and item.strip() for item in centraline):
            raise ValueError("lista centraline non valida")
        plan = ota.make_plan(ota.SquareApi(api_key()), firmware, centraline or None, bool(payload.get("force")))
        plan_id = uuid.uuid4().hex
        PLANS[plan_id] = plan
        self.send_json(HTTPStatus.OK, {"plan_id": plan_id, "plan": plan})

    def apply(self, payload: dict) -> None:
        plan = PLANS.get(str(payload.get("plan_id", "")))
        if not plan:
            raise ValueError("piano assente o dashboard riavviata: genera nuovo piano")
        target = plan["target_firmware"]
        targets = [item for item in plan["devices"] if item["status"] == "needs_update"]
        api = ota.SquareApi(api_key())
        results = []
        for item in targets:
            response = api.post("modifica_firmware", {"centralina": item["ID"], "versione": target})
            results.append({"ID": item["ID"], "ok": ota.response_ok(response), "response": response})
        self.send_json(HTTPStatus.OK, {"target_firmware": target, "results": results})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--open", action="store_true", help="apre dashboard nel browser predefinito")
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", 8765), DashboardHandler)
    print("Sense Square OTA dashboard: http://127.0.0.1:8765")
    if args.open:
        webbrowser.open("http://127.0.0.1:8765")
    server.serve_forever()


if __name__ == "__main__":
    main()
