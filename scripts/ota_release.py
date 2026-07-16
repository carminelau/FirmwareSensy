#!/usr/bin/env python3
"""Publish Sensy OTA firmware and safely assign it to compatible centraline."""

from __future__ import annotations

import argparse
import glob
import json
import mimetypes
import os
import re
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


API_BASE = "https://square.sensesquare.eu:5002"
FIRMWARE_API_BASE = "http://square.sensesquare.eu:5010"
API_KEY_ENV = "SENSE_SQUARE_APIKEY"
MAX_FIRMWARE_NAME_LENGTH = 21
FIRMWARE_RE = re.compile(
    r"^(?P<channel>BT|ST)_(?P<board>\d{4}V\d+_[A-Z0-9]+)_(?P<version>V\d+(?:_\d+)*)\.bin$"
)
BOARD_SUFFIXES = {"white": "WH", "green": "GR", "black": "BK", "red": "RD", "enea": "EN", "wh": "WH", "gr": "GR", "bk": "BK", "rd": "RD", "en": "EN"}
ID_KEYS = ("ID", "id", "id_centralina", "centralina", "device_id")
BOARD_KEYS = ("board", "suitable_board", "tipo_board", "board_name", "tipo")
FIRMWARE_KEYS = ("firmware", "firmware_name", "versione_firmware", "versione", "version")
ALL_ACCESSIBLE_CENTRALINE_QUERY = {"personal": "False"}


def firmware_name(path_or_name: str) -> str:
    name = Path(path_or_name).name
    if len(name) > MAX_FIRMWARE_NAME_LENGTH:
        raise ValueError(f"nome firmware oltre limite EEPROM ({len(name)}/{MAX_FIRMWARE_NAME_LENGTH}): {name}")
    if not FIRMWARE_RE.fullmatch(name):
        raise ValueError("nome firmware non valido: atteso BT|ST_2024V4_BK_V5_4.bin")
    return name


def board_firmware_payload(name: str) -> dict[str, str]:
    """Return set_board_firmware fields for one release binary."""
    match = FIRMWARE_RE.fullmatch(name)
    if not match:
        raise ValueError(f"nome firmware non valido: {name}")
    try:
        board = get_id_board(match.group("board"))
    except KeyError as exc:
        raise ValueError(f"board OTA non mappata: {match.group('board')}") from exc
    if not board:
        raise ValueError(f"board OTA non valida: {match.group('board')}")
    default_board = [board] if match.group("channel") == "ST" else []
    return {
        "versione": name,
        "suitable_board": json.dumps([board]),
        "default_board": json.dumps(default_board),
    }


def firmware_paths(expressions: list[str]) -> list[Path]:
    """Expand shell-independent globs; PowerShell leaves wildcards for executables."""
    paths: list[Path] = []
    for expression in expressions:
        matches = [Path(match) for match in glob.glob(expression)]
        paths.extend(matches or [Path(expression)])
    return paths


def get_id_board(value: Any) -> str | None:
    """Return exact board value firmware sends to /get_ID, e.g. 2024V4_BK."""
    if not isinstance(value, str):
        return None
    normalized = value.strip().lower().replace("-", "_").replace(" ", "_")
    direct = re.fullmatch(r"(\d{4})_?v(\d+)_([a-z0-9]+)", normalized)
    if direct and direct.group(3) in BOARD_SUFFIXES:
        return f"{direct.group(1)}V{direct.group(2)}_{BOARD_SUFFIXES[direct.group(3)]}"
    environment = re.fullmatch(r"sensy_(\d{4})_v(\d+)_([a-z0-9]+)", normalized)
    if environment and environment.group(3) in BOARD_SUFFIXES:
        return f"{environment.group(1)}V{environment.group(2)}_{BOARD_SUFFIXES[environment.group(3)]}"
    return None


def value_for(data: Any, keys: tuple[str, ...]) -> Any:
    """Find first non-empty field in a possibly wrapped API response."""
    if isinstance(data, dict):
        for key in keys:
            value = data.get(key)
            if value not in (None, "", [], {}):
                return value
        for value in data.values():
            found = value_for(value, keys)
            if found not in (None, "", [], {}):
                return found
    elif isinstance(data, list):
        for value in data:
            found = value_for(value, keys)
            if found not in (None, "", [], {}):
                return found
    return None


def device_ids(data: Any) -> list[dict[str, str]]:
    """Extract ID/alias pairs; backend returns either records or a plain ID array."""
    found: dict[str, dict[str, str]] = {}

    def visit(node: Any) -> None:
        if isinstance(node, dict):
            identifier = next((node[key] for key in ID_KEYS if node.get(key) not in (None, "")), None)
            if identifier is not None:
                device_id = str(identifier)
                alias = next((node[key] for key in ("alias", "nome", "name") if node.get(key)), "")
                found.setdefault(device_id, {"ID": device_id, "alias": str(alias)})
            for child in node.values():
                visit(child)
        elif isinstance(node, list):
            for child in node:
                if isinstance(child, str) and child:
                    found.setdefault(child, {"ID": child, "alias": ""})
                else:
                    visit(child)

    visit(data)
    return list(found.values())


def select_centraline(centraline: list[dict[str, str]], requested: list[str] | None) -> tuple[list[dict[str, str]], list[str]]:
    """Select centraline by ID or alias, preserving unmatched requested values."""
    if not requested:
        return centraline, []
    requested_by_normalized = {value.casefold(): value for value in requested}
    selected = []
    matched: set[str] = set()
    for item in centraline:
        keys = {item["ID"].casefold()}
        if item.get("alias"):
            keys.add(item["alias"].casefold())
        matches = keys & set(requested_by_normalized)
        if matches:
            selected.append(item)
            matched.update(matches)
    return selected, [value for key, value in requested_by_normalized.items() if key not in matched]


class SquareApi:
    def __init__(self, api_key: str | None, base_url: str = API_BASE) -> None:
        self.api_key = api_key
        self.base_url = base_url.rstrip("/")

    @staticmethod
    def encode_multipart(fields: dict[str, str], file_field: tuple[str, Path] | None = None) -> tuple[bytes, str]:
        boundary = f"----SensyRelease{uuid.uuid4().hex}"
        chunks: list[bytes] = []
        for key, value in fields.items():
            chunks.extend(
                (
                    f"--{boundary}\r\n".encode(),
                    f'Content-Disposition: form-data; name="{key}"\r\n\r\n'.encode(),
                    str(value).encode(),
                    b"\r\n",
                )
            )
        if file_field:
            field, path = file_field
            mime = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
            chunks.extend(
                (
                    f"--{boundary}\r\n".encode(),
                    f'Content-Disposition: form-data; name="{field}"; filename="{path.name}"\r\n'.encode(),
                    f"Content-Type: {mime}\r\n\r\n".encode(),
                    path.read_bytes(),
                    b"\r\n",
                )
            )
        chunks.append(f"--{boundary}--\r\n".encode())
        return b"".join(chunks), boundary

    def post(
        self,
        route: str,
        fields: dict[str, str] | None = None,
        file_field: tuple[str, Path] | None = None,
        base_url: str | None = None,
        include_api_key: bool = True,
    ) -> Any:
        if include_api_key and not self.api_key:
            raise ValueError(f"API key mancante: impostare {API_KEY_ENV} oppure usare --api-key")
        request_fields = dict(fields or {})
        if include_api_key:
            request_fields = {"apikey": self.api_key, **request_fields}
        payload, boundary = self.encode_multipart(request_fields, file_field)
        request = Request(
            f"{(base_url or self.base_url).rstrip('/')}/{route.lstrip('/')}",
            data=payload,
            headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
            method="POST",
        )
        try:
            with urlopen(request, timeout=45) as response:
                body = response.read().decode("utf-8", errors="replace")
        except HTTPError as exc:
            body = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"{route}: HTTP {exc.code}: {body[:500]}") from exc
        except URLError as exc:
            raise RuntimeError(f"{route}: rete non disponibile: {exc.reason}") from exc
        try:
            return json.loads(body)
        except json.JSONDecodeError:
            return {"raw_response": body}


def centraline_for_plan(api: SquareApi, requested: list[str] | None) -> tuple[list[dict[str, str]], list[str]]:
    """Query each requested ID: elenco_centraline otherwise returns only one page."""
    if not requested:
        return device_ids(api.post("elenco_centraline", ALL_ACCESSIBLE_CENTRALINE_QUERY)), []
    selected: dict[str, dict[str, str]] = {}
    not_found = []
    for selector in requested:
        candidates = device_ids(api.post("elenco_centraline", {**ALL_ACCESSIBLE_CENTRALINE_QUERY, "query": selector}))
        matches, _ = select_centraline(candidates, [selector])
        # Query response is normally an ID array; accept a sole unambiguous result for an alias query.
        if not matches and len(candidates) == 1:
            matches = candidates
        if matches:
            for item in matches:
                selected.setdefault(item["ID"], item)
        else:
            not_found.append(selector)
    return list(selected.values()), not_found


def response_ok(response: Any) -> bool:
    if not isinstance(response, dict):
        return False
    code = response.get("response_code")
    return code in (200, "200")


def make_plan(
    api: SquareApi,
    target: str,
    requested_centraline: list[str] | None = None,
    force_unknown_board: bool = False,
) -> dict[str, Any]:
    target_board = get_id_board(FIRMWARE_RE.fullmatch(target).group("board"))  # validated by caller
    centraline, not_found = centraline_for_plan(api, requested_centraline)
    devices: list[dict[str, str]] = []
    for item in centraline:
        info = api.post("informazioni_centralina", {"ID": item["ID"]})
        current_board = get_id_board(value_for(info, BOARD_KEYS))
        current_firmware = value_for(info, FIRMWARE_KEYS)
        current_firmware = str(current_firmware) if current_firmware is not None else ""
        board_source = "backend" if current_board else ""
        firmware_match = FIRMWARE_RE.fullmatch(current_firmware)
        if not current_board and firmware_match:
            current_board = get_id_board(firmware_match.group("board"))
            board_source = "firmware"
        status = "unknown"
        forced = False
        if current_board == target_board:
            status = "up_to_date" if current_firmware == target else "needs_update"
        elif force_unknown_board and requested_centraline and not current_board:
            status = "needs_update"
            forced = True
            board_source = "forced"
        devices.append(
            {
                **item,
                "board": current_board or "",
                "board_source": board_source,
                "current_firmware": current_firmware,
                "status": status,
                "forced": forced,
            }
        )
    return {
        "created_at": datetime.now(timezone.utc).isoformat(),
        "target_firmware": target,
        "target_board": target_board,
        "requested_centraline": requested_centraline or [],
        "force_unknown_board": force_unknown_board,
        "not_found_centraline": not_found,
        "devices": devices,
    }


def write_json(path: Path, data: Any) -> None:
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def command_upload(args: argparse.Namespace, api: SquareApi) -> int:
    failed = 0
    for path in firmware_paths(args.firmware):
        if not path.is_file():
            raise ValueError(f"file firmware non trovato: {path}")
        name = firmware_name(path.name)
        response = api.post(
            "set_board_firmware",
            board_firmware_payload(name),
            ("contenuto", path),
            base_url=FIRMWARE_API_BASE,
            include_api_key=False,
        )
        if not response_ok(response):
            failed += 1
            print(f"upload fallito: {name}", file=sys.stderr)
        else:
            channel = FIRMWARE_RE.fullmatch(name).group("channel")
            print(f"firmware registrato: {name} | default={'si' if channel == 'ST' else 'no'}")
    return 1 if failed else 0


def command_plan(args: argparse.Namespace, api: SquareApi) -> int:
    target = firmware_name(args.firmware)
    plan = make_plan(api, target, args.centraline, args.force)
    output = Path(args.output) if args.output else Path(f"ota-plan-{target.removesuffix('.bin')}.json")
    write_json(output, plan)
    update_count = sum(item["status"] == "needs_update" for item in plan["devices"])
    unknown_count = sum(item["status"] == "unknown" for item in plan["devices"])
    forced_count = sum(item["forced"] for item in plan["devices"])
    print(f"piano: {output} | da aggiornare={update_count} | forzate={forced_count} | esclusi/ignoti={unknown_count} | non trovate={len(plan['not_found_centraline'])}")
    return 0


def command_apply(args: argparse.Namespace, api: SquareApi) -> int:
    plan = json.loads(Path(args.plan).read_text(encoding="utf-8"))
    target = firmware_name(str(plan.get("target_firmware", "")))
    if args.confirm != target:
        raise ValueError(f"conferma richiesta: --confirm {target}")
    targets = [item for item in plan.get("devices", []) if item.get("status") == "needs_update"]
    if not targets:
        print("nessuna centralina da aggiornare")
        return 0
    results = []
    for item in targets:
        response = api.post("modifica_firmware", {"centralina": str(item["ID"]), "versione": target})
        results.append({"ID": item["ID"], "ok": response_ok(response), "response": response})
    result_path = Path(args.plan).with_suffix(".result.json")
    write_json(result_path, {"target_firmware": target, "results": results})
    failed = sum(not item["ok"] for item in results)
    print(f"assegnazioni OTA completate={len(results) - failed} fallite={failed} | risultato: {result_path}")
    return 1 if failed else 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    root.add_argument("--api-key", default=os.environ.get(API_KEY_ENV), help=f"default: ${API_KEY_ENV}")
    root.add_argument("--base-url", default=API_BASE, help=argparse.SUPPRESS)
    commands = root.add_subparsers(dest="command", required=True)

    upload = commands.add_parser("upload", help="registra binari per board sul backend OTA")
    upload.add_argument("--firmware", required=True, nargs="+", help="uno o piu file .bin release")

    plan = commands.add_parser("plan", help="genera elenco centraline compatibili da aggiornare")
    plan.add_argument("--firmware", required=True, help="file o nome firmware release")
    plan.add_argument("--centraline", nargs="+", help="array ID o alias: limita piano e apply a queste centraline")
    plan.add_argument("--force", action="store_true", help="con --centraline, include board assente; non supera incompatibilita nota")
    plan.add_argument("--output")

    apply = commands.add_parser("apply", help="assegna firmware a tutte centraline del piano")
    apply.add_argument("--plan", required=True)
    apply.add_argument("--confirm", required=True, help="deve coincidere esattamente col firmware nel piano")
    return root


def main() -> int:
    args = parser().parse_args()
    try:
        api = SquareApi(args.api_key, args.base_url)
        return {"upload": command_upload, "plan": command_plan, "apply": command_apply}[args.command](args, api)
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as exc:
        print(f"ERRORE: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
