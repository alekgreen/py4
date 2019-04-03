#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

cd "$ROOT_DIR"

make >/dev/null
./py4 benchmarks/cpu_bound.p4 > "$TMP_DIR/cpu_bound.c"
gcc -O2 -std=c11 "$TMP_DIR/cpu_bound.c" -o "$TMP_DIR/cpu_bound_c"
go build -o "$TMP_DIR/cpu_bound_go" benchmarks/cpu_bound.go

HAS_PYPY=0
if command -v pypy3 >/dev/null 2>&1; then
    HAS_PYPY=1
fi

printf 'Python result: '
python3 benchmarks/cpu_bound.py
if [ "$HAS_PYPY" -eq 1 ]; then
    printf 'PyPy result: '
    pypy3 benchmarks/cpu_bound.py
fi
printf 'Go result: '
"$TMP_DIR/cpu_bound_go"
printf 'py4 result: '
"$TMP_DIR/cpu_bound_c"
printf '\n'

printf 'Python timings (s):\n'
for _ in 1 2 3; do
    /usr/bin/time -f '%e' python3 benchmarks/cpu_bound.py >/dev/null
done 2>&1

if [ "$HAS_PYPY" -eq 1 ]; then
    printf '\nPyPy timings (s):\n'
    for _ in 1 2 3; do
        /usr/bin/time -f '%e' pypy3 benchmarks/cpu_bound.py >/dev/null
    done 2>&1
fi

printf '\nGo timings (s):\n'
for _ in 1 2 3; do
    /usr/bin/time -f '%e' "$TMP_DIR/cpu_bound_go" >/dev/null
done 2>&1

printf '\npy4 timings (s):\n'
for _ in 1 2 3; do
    /usr/bin/time -f '%e' "$TMP_DIR/cpu_bound_c" >/dev/null
done 2>&1
