#!/usr/bin/env sh
set -eu

python3 scripts/build_matrix.py --check "$@"
