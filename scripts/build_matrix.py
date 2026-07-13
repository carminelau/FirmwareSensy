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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fallisce quando supera budget")
    parser.add_argument("--output", default=".pio/size-report.json")
    args = parser.parse_args()

    report = {environment: build(environment) for environment in ENVIRONMENTS}
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    failed = False
    for environment, sizes in report.items():
        budget = BUDGETS[environment]
        print(f"{environment}: flash={sizes['flash']}/{budget['flash']} ram={sizes['ram']}/{budget['ram']}")
        failed |= sizes["flash"] > budget["flash"] or sizes["ram"] > budget["ram"]
    return 1 if args.check and failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
