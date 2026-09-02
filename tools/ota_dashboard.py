#!/usr/bin/env python3
"""Sense Square OTA dashboard. Defaults to localhost; use --lan for trusted LAN access."""

from __future__ import annotations

import argparse
import configparser
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import uuid
import webbrowser
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlparse


ROOT = Path(__file__).resolve().parents[1]
WEB_ROOT = ROOT / "tools" / "ota_dashboard_web"
CONFIG_PATH = Path(os.environ.get("APPDATA", str(Path.home()))) / "SenseSquare" / "ota_dashboard.json"
sys.path.insert(0, str(ROOT / "scripts"))
import ota_release as ota  # noqa: E402

PLANS: dict[str, dict] = {}
ACTIVITY_LOGS: list[dict] = []
DASHBOARD_API_KEY: str | None = None
ENVIRONMENT_BOARDS = {
    "sensy_2024_V4_black": "2024V4_BK", "sensy_2024_V4_green": "2024V4_GR",
    "sensy_2024_V3_red": "2024V3_RD", "sensy_2024_V2_ENEA": "2024V2_EN",
    "sensy_2024_V1_green": "2024V1_GR", "sensy_2023_V2_black": "2023V2_BK",
    "sensy_2023_V1_green": "2023V1_GR", "sensy_2021_V4_white": "2021V4_WH",
}


def log_safe(value: Any) -> Any:
    if isinstance(value, dict):
        return {key: "***" if key.lower() in {"apikey", "api_key"} else log_safe(item) for key, item in value.items()}
    if isinstance(value, list):
        return [log_safe(item) for item in value]
    if isinstance(value, str) and len(value) > 12000:
        return value[:12000] + "… [troncato]"
    return value


class LoggedSquareApi(ota.SquareApi):
    def post(self, route: str, fields: dict | None = None, file_field=None, base_url: str | None = None, include_api_key: bool = True) -> Any:
        started = time.perf_counter()
        target = (base_url or self.base_url).rstrip("/")
        params = dict(fields or {})
        if file_field:
            params[file_field[0]] = {"file": file_field[1].name, "bytes": file_field[1].stat().st_size}
        try:
            response = super().post(route, fields, file_field, base_url, include_api_key)
            status = response.get("response_code", 200) if isinstance(response, dict) else 200
            entry = {"time": datetime.now(timezone.utc).isoformat(), "type": "SENSE SQUARE", "method": "POST", "route": f"{target}/{route.lstrip('/')}", "status": status, "ok": ota.response_ok(response), "duration_ms": round((time.perf_counter() - started) * 1000), "params": log_safe(params), "response": log_safe(response), "message": ""}
            ACTIVITY_LOGS.insert(0, entry)
            del ACTIVITY_LOGS[300:]
            return response
        except Exception as exc:
            ACTIVITY_LOGS.insert(0, {"time": datetime.now(timezone.utc).isoformat(), "type": "SENSE SQUARE", "method": "POST", "route": f"{target}/{route.lstrip('/')}", "status": "ERR", "ok": False, "duration_ms": round((time.perf_counter() - started) * 1000), "params": log_safe(params), "response": {}, "message": str(exc)})
            del ACTIVITY_LOGS[300:]
            raise


def firmware_api() -> ota.SquareApi:
    return LoggedSquareApi(api_key(), ota.FIRMWARE_API_BASE)


def normalized_firmware_name(value: Any) -> str:
    """Backend catalogues may omit the .bin suffix stored in local release names."""
    name = str(value).strip()
    candidate = name if name.endswith(".bin") else f"{name}.bin"
    return candidate if ota.FIRMWARE_RE.fullmatch(candidate) else ""


def firmware_catalog(board: str | None = None) -> list[dict]:
    """Load all firmware, or one board; support legacy suitable_board names."""
    response = firmware_api().post("lista_firmware", {"board": board} if board else None)
    result = response.get("result", []) if isinstance(response, dict) else []
    if board and (not ota.response_ok(response) or not isinstance(result, list)):
        # Old records used environment names. Keep board filter usable while data is migrated.
        response = firmware_api().post("lista_firmware")
        result = response.get("result", []) if isinstance(response, dict) else []
    if not ota.response_ok(response) or not isinstance(result, list):
        raise ValueError("catalogo firmware backend non disponibile")
    catalog: dict[str, dict] = {}
    for item in result:
        firmware = {"versione": item} if isinstance(item, str) else item if isinstance(item, dict) else {}
        version = normalized_firmware_name(firmware.get("versione", ""))
        match = ota.FIRMWARE_RE.fullmatch(version)
        if not match:
            continue
        suitable = firmware.get("suitable_board", [])
        suitable = suitable if isinstance(suitable, list) else [suitable]
        compatible = {ota.get_id_board(value) for value in suitable}
        compatible.discard(None)
        if board and board not in compatible and match.group("board") != board:
            continue
        catalog[version] = {
            "versione": version,
            "suitable_board": suitable,
            "default_board": firmware.get("default_board", []),
            "note": firmware.get("note", ""),
        }
    return [catalog[name] for name in sorted(catalog, reverse=True)]


def platformio_boards(root: Path) -> list[dict]:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.read(root / "platformio.ini", encoding="utf-8")
    boards = []
    for section in parser.sections():
        if not section.startswith("env:"):
            continue
        environment = section.removeprefix("env:")
        board = ENVIRONMENT_BOARDS.get(environment)
        if not board:
            continue
        flags: dict[str, str] = {}
        for line in parser.get(section, "build_flags", fallback="").splitlines():
            line = line.strip()
            if line.startswith("-D") and "=" in line:
                key, value = line[2:].split("=", 1)
                flags[key] = value.strip('\\"')
        boards.append({"versione": board, "anno": flags.get("YEAR", board[:4]), "details": {"environment": environment, "platformio_board": parser.get(section, "board", fallback=""), "pins": flags, "relay": flags.get("RELAY")}})
    return boards


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


def next_release_version(board: str | None) -> str:
    versions: list[tuple[int, ...]] = []
    for firmware in firmware_catalog(board):
        name = firmware["versione"]
        match = ota.FIRMWARE_RE.fullmatch(name)
        if match and (board is None or match.group("board") == board):
            versions.append(tuple(int(part) for part in match.group("version")[1:].split("_")))
    if not versions:
        raise ValueError("nessun firmware backend per board: inserire prima versione manuale")
    latest = list(max(versions))
    latest[-1] += 1
    return "V" + "_".join(map(str, latest))


def release_path(name: str) -> Path:
    safe_name = ota.firmware_name(name)
    directory = firmware_dir()
    path = (directory / safe_name).resolve()
    if path.parent != directory or not path.is_file():
        raise ValueError("firmware non disponibile nel percorso configurato")
    return path


def git_branches(root: Path) -> tuple[list[str], str]:
    result = subprocess.run(
        ["git", "-C", str(root), "for-each-ref", "--format=%(refname:short)", "refs/heads"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if result.returncode:
        raise ValueError("progetto firmware non è repository Git")
    current = subprocess.run(
        ["git", "-C", str(root), "branch", "--show-current"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    return sorted(line.strip() for line in result.stdout.splitlines() if line.strip()), current.stdout.strip()


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
        if urlparse(self.path).path.startswith("/api/") and urlparse(self.path).path != "/api/logs":
            ACTIVITY_LOGS.insert(0, {"time": datetime.now(timezone.utc).isoformat(), "method": self.command, "route": urlparse(self.path).path, "status": int(status), "ok": int(status) < 400, "duration_ms": round((time.perf_counter() - getattr(self, "_started_at", time.perf_counter())) * 1000), "message": payload.get("error") or payload.get("message", ""), "params": log_safe(getattr(self, "_request_params", {})), "response": log_safe(payload)})
            del ACTIVITY_LOGS[300:]

    def read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", "0"))
        if length > 1_000_000:
            raise ValueError("richiesta troppo grande")
        value = json.loads(self.rfile.read(length).decode("utf-8"))
        if not isinstance(value, dict):
            raise ValueError("JSON oggetto richiesto")
        return value

    def do_GET(self):
        self._started_at = time.perf_counter()
        self._request_params = {"query": urlparse(self.path).query}
        path = urlparse(self.path).path
        if path == "/api/logs":
            errors = sum(not item["ok"] for item in ACTIVITY_LOGS)
            self.send_json(HTTPStatus.OK, {"logs": ACTIVITY_LOGS, "total": len(ACTIVITY_LOGS), "errors": errors})
            return
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
                board = parse_qs(urlparse(self.path).query).get("board", [None])[0] or None
                firmwares = firmware_catalog(board)
                self.send_json(HTTPStatus.OK, {"releases": [item["versione"] for item in firmwares], "firmwares": firmwares, "boards": sorted(set(ENVIRONMENT_BOARDS.values()))})
            except (ValueError, RuntimeError) as exc:
                self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
            except Exception as exc:
                self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": f"errore server: {exc}"})
            return
        if path == "/api/branches":
            try:
                branches, current = git_branches(project_root())
                self.send_json(HTTPStatus.OK, {"branches": branches, "current_branch": current})
            except ValueError as exc:
                self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
            return
        if path == "/api/release-notes":
            result = subprocess.run(["git", "-C", str(project_root()), "log", "-n", "12", "--pretty=format:%s"], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.send_json(HTTPStatus.OK, {"notes": result.stdout.strip()})
            return
        if path == "/api/firmware-notes":
            response = firmware_api().post("lista_firmware")
            result = response.get("result", []) if isinstance(response, dict) else []
            if not ota.response_ok(response) or not isinstance(result, list):
                self.send_json(HTTPStatus.BAD_REQUEST, {"error": "catalogo firmware backend non disponibile"})
                return
            notes = subprocess.run(["git", "-C", str(project_root()), "log", "-n", "12", "--pretty=format:%s"], text=True, stdout=subprocess.PIPE).stdout.strip()
            self.send_json(HTTPStatus.OK, {"firmwares": result, "suggested_notes": notes})
            return
        if self.path == "/":
            self.path = "/index.html"
        return super().do_GET()

    def do_POST(self):
        self._started_at = time.perf_counter()
        try:
            payload = self.read_json()
            self._request_params = payload
            path = urlparse(self.path).path
            if path == "/api/build":
                return self.build(payload)
            if path == "/api/config":
                return self.config(payload)
            if path == "/api/upload":
                return self.upload(payload)
            if path == "/api/sync-boards":
                return self.sync_boards()
            if path == "/api/firmware-notes":
                return self.update_firmware_notes(payload)
            if path == "/api/plan":
                return self.plan(payload)
            if path == "/api/apply":
                return self.apply(payload)
            self.send_json(HTTPStatus.NOT_FOUND, {"error": "endpoint non trovato"})
        except (ValueError, RuntimeError, json.JSONDecodeError) as exc:
            self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})
        except Exception as exc:
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": f"errore server: {exc}"})

    def do_DELETE(self):
        self._started_at = time.perf_counter()
        self._request_params = {}
        if urlparse(self.path).path == "/api/logs":
            ACTIVITY_LOGS.clear()
            self.send_json(HTTPStatus.OK, {"logs": [], "total": 0, "errors": 0})
            return
        self.send_json(HTTPStatus.NOT_FOUND, {"error": "endpoint non trovato"})

    def build(self, payload: dict) -> None:
        root = project_root()
        build_script = root / "scripts" / "build_matrix.py"
        if not build_script.is_file():
            raise ValueError("progetto firmware non valido: scripts/build_matrix.py assente")
        branches, current_branch = git_branches(root)
        selected_branch = payload.get("branch") or current_branch
        if selected_branch not in branches:
            raise ValueError("branch non disponibile nel progetto firmware")
        build_root = root
        worktree_path: Path | None = None
        if selected_branch != current_branch:
            worktree_path = Path(tempfile.mkdtemp(prefix="sense-square-ota-"))
            shutil.rmtree(worktree_path)
            checkout = subprocess.run(
                ["git", "-C", str(root), "worktree", "add", "--force", "--quiet", str(worktree_path), selected_branch],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            )
            if checkout.returncode:
                raise RuntimeError(f"worktree branch non disponibile: {checkout.stdout.strip()}")
            build_root = worktree_path
            build_script = build_root / "scripts" / "build_matrix.py"
        command = [sys.executable, str(build_script), "--check", "--artifacts-dir", str(firmware_dir())]
        channel = payload.get("channel")
        if payload.get("branch_release"):
            version = payload.get("version")
            if channel in {"beta", "stable"}:
                selected_env = next(iter(payload.get("environments", [])), None)
                version = next_release_version(ENVIRONMENT_BOARDS.get(selected_env))
            if channel not in {"beta", "stable"} or not isinstance(version, str):
                raise ValueError("selezionare canale")
            command.extend(["--channel", channel, "--version", version])
        else:
            version = payload.get("version")
            if channel not in {"beta", "stable"} or not isinstance(version, str):
                raise ValueError("selezionare canale e versione")
            command.extend(["--channel", channel, "--version", version])
        for environment in payload.get("environments", []):
            if not isinstance(environment, str) or not environment.replace("_", "").isalnum():
                raise ValueError(f"ambiente non valido: {environment}")
            command.extend(["--env", environment])
        try:
            process = subprocess.run(command, cwd=build_root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        finally:
            if worktree_path:
                subprocess.run(["git", "-C", str(root), "worktree", "remove", "--force", str(worktree_path)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                shutil.rmtree(worktree_path, ignore_errors=True)
        self.send_json(HTTPStatus.OK if process.returncode == 0 else HTTPStatus.BAD_REQUEST, {
            "ok": process.returncode == 0,
            "branch": selected_branch,
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
        api = firmware_api()
        results = []
        for name in names:
            firmware = release_path(str(name))
            route = "modifica_firmware" if payload.get("update_existing") else "inserimento_firmware"
            response = api.post(
                route,
                {**ota.board_firmware_payload(firmware.name), "note": str(payload.get("notes", ""))},
                ("contenuto", firmware),
            )
            results.append({"firmware": firmware.name, "ok": ota.response_ok(response), "response": response})
        self.send_json(HTTPStatus.OK, {"results": results})

    def sync_boards(self) -> None:
        api = firmware_api()
        result = []
        for board in platformio_boards(project_root()):
            response = api.post("inserimento_board", board)
            result.append({"board": board["versione"], "ok": ota.response_ok(response), "response": response})
        self.send_json(HTTPStatus.OK, {"results": result})

    def update_firmware_notes(self, payload: dict) -> None:
        version = ota.firmware_name(str(payload.get("versione", "")))
        note = str(payload.get("note", "")).strip()
        if not note:
            raise ValueError("nota firmware richiesta")
        response = firmware_api().post("modifica_firmware", {"versione": version, "note": note})
        self.send_json(HTTPStatus.OK, {"versione": version, "ok": ota.response_ok(response), "response": response})

    def plan(self, payload: dict) -> None:
        firmware = ota.firmware_name(str(payload.get("firmware", "")))
        centraline = payload.get("centraline") or []
        if not isinstance(centraline, list) or not all(isinstance(item, str) and item.strip() for item in centraline):
            raise ValueError("lista centraline non valida")
        plan = ota.make_plan(LoggedSquareApi(api_key()), firmware, centraline or None, bool(payload.get("force")))
        plan_id = uuid.uuid4().hex
        PLANS[plan_id] = plan
        self.send_json(HTTPStatus.OK, {"plan_id": plan_id, "plan": plan})

    def apply(self, payload: dict) -> None:
        plan = PLANS.get(str(payload.get("plan_id", "")))
        if not plan:
            raise ValueError("piano assente o dashboard riavviata: genera nuovo piano")
        target = plan["target_firmware"]
        targets = [item for item in plan["devices"] if item["status"] == "needs_update"]
        api = LoggedSquareApi(api_key())
        results = []
        for item in targets:
            response = api.post("modifica_firmware", {"centralina": item["ID"], "versione": target})
            results.append({"ID": item["ID"], "ok": ota.response_ok(response), "response": response})
        self.send_json(HTTPStatus.OK, {"target_firmware": target, "results": results})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--open", action="store_true", help="apre dashboard nel browser predefinito")
    parser.add_argument("--lan", action="store_true", help="espone dashboard sulla rete locale fidata")
    parser.add_argument("--host", default=None, help="indirizzo ascolto, es. 192.168.1.25")
    parser.add_argument("--port", type=int, default=8765, help="porta dashboard")
    args = parser.parse_args()
    host = args.host or ("0.0.0.0" if args.lan else "127.0.0.1")
    server = ThreadingHTTPServer((host, args.port), DashboardHandler)
    local_url = f"http://127.0.0.1:{args.port}"
    print(f"Sense Square OTA dashboard: {local_url}")
    if host == "0.0.0.0":
        addresses = sorted({item[4][0] for item in socket.getaddrinfo(socket.gethostname(), None, family=socket.AF_INET) if not item[4][0].startswith("127.")})
        for address in addresses:
            print(f"LAN (rete fidata): http://{address}:{args.port}")
    if args.open:
        webbrowser.open(local_url)
    server.serve_forever()


if __name__ == "__main__":
    main()
