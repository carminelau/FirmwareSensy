#!/usr/bin/env python3
"""Validate heap and task-stack stability from a FW_LOG_LEVEL=3 serial log."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


HEAP_RE = re.compile(r"Free heap:\s*(\d+)\s*bytes")
STACK_RE = re.compile(r"Stack watermark monitor=(\d+) watchdog=(\d+) gps=(\d+) bytes")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--min-samples", type=int, default=20)
    parser.add_argument("--max-heap-span", type=int, default=1024)
    parser.add_argument("--min-stack", type=int, default=2048)
    parser.add_argument("--allow-missing-stack", action="store_true")
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    heaps = [int(value) for value in HEAP_RE.findall(text)]
    stacks = [tuple(map(int, values)) for values in STACK_RE.findall(text)]

    failures: list[str] = []
    if len(heaps) < args.min_samples:
        failures.append(f"campioni heap insufficienti: {len(heaps)} < {args.min_samples}")
    elif max(heaps) - min(heaps) >= args.max_heap_span:
        failures.append(f"span heap {max(heaps) - min(heaps)} >= {args.max_heap_span} bytes")

    if not stacks and not args.allow_missing_stack:
        failures.append("watermark stack assente: usare FW_LOG_LEVEL=3")
    elif stacks:
        minimum = min(min(sample) for sample in stacks)
        if minimum < args.min_stack:
            failures.append(f"watermark stack minimo {minimum} < {args.min_stack} bytes")

    if heaps:
        print(f"heap samples={len(heaps)} first={heaps[0]} last={heaps[-1]} span={max(heaps)-min(heaps)}")
    if stacks:
        print(f"stack samples={len(stacks)} minimum={min(min(sample) for sample in stacks)}")
    for failure in failures:
        print(f"FAIL: {failure}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
