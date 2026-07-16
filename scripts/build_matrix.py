#!/usr/bin/env python3
"""Build all hardware environments, report size, enforce regression budgets."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


ENVIRONMENTS = (
    "sensy_2021_V4_white",
    "sensy_2023_V1_green",
    "sensy_2023_V2_black",
    "sensy_2024_V1_green",
    "sensy_2024_V2_ENEA",
    "sensy_2024_V3_red",
    "sensy_2024_V4_green",
    "sensy_2024_V4_black",
)

# Codici usati dal backend OTA. Tenerli espliciti evita nomi ambigui per i
# colori e mantiene il nome completo (compresa l'estensione .bin) entro i 21
# byte riservati in EEPROM dal firmware.
RELEASE_BOARD_CODES = {
    "sensy_2021_V4_white": "2021V4_WH",
    "sensy_2023_V1_green": "2023V1_GR",
    "sensy_2023_V2_black": "2023V2_BK",
    "sensy_2024_V1_green": "2024V1_GR",
    "sensy_2024_V2_ENEA": "2024V2_EN",
    "sensy_2024_V3_red": "2024V3_RD",
    "sensy_2024_V4_green": "2024V4_GR",
    "sensy_2024_V4_black": "2024V4_BK",
}

CHANNEL_CODES = {"beta": "BT", "stable": "ST"}
FIRMWARE_NAME_MAX_LENGTH = 21
RELEASE_VERSION_RE = re.compile(r"V\d+(?:_\d+)*$")
ARTIFACT_VERSION_RE = re.compile(r"^(?:BT|ST)_.+_(V\d+(?:_\d+)*)\.bin$")

BUDGETS = {
    env: {"flash": 1_073_000 if not env.startswith("sensy_2024") else 1_055_000, "ram": 65_300}
    for env in ENVIRONMENTS
}

RAM_RE = re.compile(r"RAM:.*used\s+(\d+)\s+bytes")
FLASH_RE = re.compile(r"Flash:.*used\s+(\d+)\s+bytes")
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


def platformio_command() -> str:
    configured = os.environ.get("PLATFORMIO_CMD")
    if configured:
        return configured
    found = shutil.which("pio") or shutil.which("platformio")
    if found:
        return found
    windows = Path.home() / ".platformio" / "penv" / "Scripts" / "pio.exe"
    if windows.exists():
        return str(windows)
    raise RuntimeError("PlatformIO non trovato; impostare PLATFORMIO_CMD")


def build(environment: str) -> dict[str, int]:
    process = subprocess.run(
        [platformio_command(), "run", "-e", environment],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env={**os.environ, "PLATFORMIO_SETTING_ENABLE_TELEMETRY": "No"},
    )
    output = ANSI_RE.sub("", process.stdout)
    sys.stdout.write(output)
    if process.returncode != 0:
        raise RuntimeError(f"build fallita: {environment}")
    ram = RAM_RE.search(output)
    flash = FLASH_RE.search(output)
    if not ram or not flash:
        raise RuntimeError(f"dimensioni non trovate: {environment}")
    return {"ram": int(ram.group(1)), "flash": int(flash.group(1))}


def release_filename(environment: str, channel: str, version: str) -> str:
    """Return OTA filename, including .bin, validated against EEPROM limit."""
    try:
        channel_code = CHANNEL_CODES[channel]
        board_code = RELEASE_BOARD_CODES[environment]
    except KeyError as exc:
        raise ValueError(f"canale o ambiente non supportato: {exc.args[0]}") from exc
    if not RELEASE_VERSION_RE.fullmatch(version):
        raise ValueError("versione non valida: usare formato V5_4")
    filename = f"{channel_code}_{board_code}_{version}.bin"
    if len(filename) > FIRMWARE_NAME_MAX_LENGTH:
        raise ValueError(
            f"nome firmware troppo lungo ({len(filename)}/{FIRMWARE_NAME_MAX_LENGTH}): {filename}"
        )
    return filename


def increment_version(version: str) -> str:
    """Increment final numeric component: V5_4 becomes V5_5."""
    if not RELEASE_VERSION_RE.fullmatch(version):
        raise ValueError("versione non valida: usare formato V5_4")
    parts = [int(part) for part in version[1:].split("_")]
    parts[-1] += 1
    return "V" + "_".join(str(part) for part in parts)


def latest_release_version(artifacts_dir: Path) -> str | None:
    versions = []
    if artifacts_dir.is_dir():
        for path in artifacts_dir.glob("*.bin"):
            match = ARTIFACT_VERSION_RE.fullmatch(path.name)
            if match:
                version = match.group(1)
                versions.append((tuple(int(part) for part in version[1:].split("_")), version))
    return max(versions, default=((), None))[1]


def current_git_branch() -> str:
    process = subprocess.run(
        ["git", "branch", "--show-current"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return process.stdout.strip() if process.returncode == 0 else ""


def export_release_binary(environment: str, channel: str, version: str, artifacts_dir: Path) -> Path:
    source = Path(".pio") / "build" / environment / "firmware.bin"
    if not source.is_file():
        raise RuntimeError(f"binario compilato non trovato: {source}")
    destination = artifacts_dir / release_filename(environment, channel, version)
    shutil.copy2(source, destination)
    print(f"release: {destination}")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fallisce quando supera budget")
    parser.add_argument("--output", default=".pio/size-report.json")
    parser.add_argument("--env", choices=ENVIRONMENTS, action="append", help="compila solo ambiente indicato")
    parser.add_argument("--channel", choices=CHANNEL_CODES, help="canale OTA: beta o stable")
    parser.add_argument("--version", help="versione OTA, es. V5_4")
    parser.add_argument("--artifacts-dir", default="dist", help="cartella binari OTA rinominati")
    parser.add_argument("--branch-release", action="store_true", help="branch non-main: BT e incremento automatico ultima versione in dist")
    args = parser.parse_args()

    if bool(args.channel) != bool(args.version):
        parser.error("--channel e --version devono essere usati insieme")
    if args.version and not RELEASE_VERSION_RE.fullmatch(args.version):
        parser.error("--version deve avere formato V5_4")
    artifacts_dir = Path(args.artifacts_dir)
    if args.branch_release:
        if args.channel or args.version:
            parser.error("--branch-release non accetta --channel o --version")
        branch = current_git_branch()
        if not branch or branch == "main":
            parser.error("--branch-release richiede branch Git diverso da main")
        previous = latest_release_version(artifacts_dir)
        if not previous:
            parser.error("nessuna release in dist: usare --channel beta --version V5_4 una prima volta")
        args.channel = "beta"
        args.version = increment_version(previous)
        print(f"branch release: {branch} | {previous} -> {args.version} | BT")

    environments = tuple(args.env) if args.env else ENVIRONMENTS
    report = {}
    releases = []
    for environment in environments:
        report[environment] = build(environment)
        if args.channel:
            artifacts_dir.mkdir(parents=True, exist_ok=True)
            releases.append(export_release_binary(environment, args.channel, args.version, artifacts_dir))

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {
                "sizes": report,
                "releases": [str(path) for path in releases],
            }
            if args.channel
            else report,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )

    failed = False
    for environment, sizes in report.items():
        budget = BUDGETS[environment]
        print(f"{environment}: flash={sizes['flash']}/{budget['flash']} ram={sizes['ram']}/{budget['ram']}")
        failed |= sizes["flash"] > budget["flash"] or sizes["ram"] > budget["ram"]
    return 1 if args.check and failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
