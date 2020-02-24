#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
export GOCACHE="$TMP_DIR/go-cache"
PY4_BACKEND="${PY4_BACKEND:-gcc}"

run_three_times() {
    local label="$1"
    shift

    printf '\n%s timings (s):\n' "$label"
    for _ in 1 2 3; do
        python3 benchmarks/time_command.py "$@"
    done
}

run_benchmark() {
    local name="$1"
    local py_file="$2"
    local p4_file="$3"
    local go_file="$4"
    local c_file="$5"
    local py4_o2_bin="$TMP_DIR/${name}_py4_o2"
    local py4_o3_bin="$TMP_DIR/${name}_py4_o3"
    local go_bin="$TMP_DIR/${name}_go"
    local c_bin="$TMP_DIR/${name}_c"

    printf '== %s ==\n' "$name"

    ./py4 --release --backend "$PY4_BACKEND" -o "$py4_o2_bin" "$p4_file"
    ./py4 --opt-level 3 --backend "$PY4_BACKEND" -o "$py4_o3_bin" "$p4_file"
    go build -o "$go_bin" "$go_file"
    gcc -O2 -std=c11 "$c_file" -o "$c_bin"

    printf 'Python result: '
    python3 "$py_file"
    if [ "$HAS_PYPY" -eq 1 ]; then
        printf 'PyPy result: '
        pypy3 "$py_file"
    fi
    printf 'Go result: '
    "$go_bin"
    printf 'Direct C result: '
    "$c_bin"
    printf 'py4 O2 result: '
    "$py4_o2_bin"
    printf 'py4 O3 result: '
    "$py4_o3_bin"
    printf '\n'

    run_three_times "Python" python3 "$py_file"

    if [ "$HAS_PYPY" -eq 1 ]; then
        run_three_times "PyPy" pypy3 "$py_file"
    fi

    run_three_times "Go" "$go_bin"
    run_three_times "Direct C" "$c_bin"
    run_three_times "py4-O2" "$py4_o2_bin"
    run_three_times "py4-O3" "$py4_o3_bin"
    printf '\n'
}

run_json_benchmark() {
    local name="json_roundtrip"
    local py_file="benchmarks/json_roundtrip.py"
    local pydantic_file="benchmarks/json_roundtrip_pydantic.py"
    local p4_file="benchmarks/json_roundtrip.p4"
    local go_file="benchmarks/json_roundtrip.go"
    local c_file="benchmarks/json_roundtrip.c"
    local py4_o2_bin="$TMP_DIR/${name}_py4_o2"
    local py4_o3_bin="$TMP_DIR/${name}_py4_o3"
    local go_bin="$TMP_DIR/${name}_go"
    local c_bin="$TMP_DIR/${name}_c"

    printf '== %s ==\n' "$name"

    ./py4 --release --backend "$PY4_BACKEND" -o "$py4_o2_bin" "$p4_file"
    ./py4 --opt-level 3 --backend "$PY4_BACKEND" -o "$py4_o3_bin" "$p4_file"
    go build -o "$go_bin" "$go_file"
    gcc -O2 -std=c11 "$c_file" runtime/vendor/cjson/cJSON.c -I. -o "$c_bin"

    printf 'Python result: '
    python3 "$py_file"
    if [ "$HAS_PYPY" -eq 1 ]; then
        printf 'PyPy result: '
        pypy3 "$py_file"
    fi
    if [ "$HAS_PYDANTIC" -eq 1 ]; then
        printf 'Pydantic result: '
        "$PYDANTIC_PY" "$pydantic_file"
    fi
    printf 'Go result: '
    "$go_bin"
    printf 'Direct C result: '
    "$c_bin"
    printf 'py4 O2 result: '
    "$py4_o2_bin"
    printf 'py4 O3 result: '
    "$py4_o3_bin"
    printf '\n'

    run_three_times "Python" python3 "$py_file"

    if [ "$HAS_PYPY" -eq 1 ]; then
        run_three_times "PyPy" pypy3 "$py_file"
    fi

    if [ "$HAS_PYDANTIC" -eq 1 ]; then
        run_three_times "Pydantic" "$PYDANTIC_PY" "$pydantic_file"
    fi

    run_three_times "Go" "$go_bin"
    run_three_times "Direct C" "$c_bin"
    run_three_times "py4-O2" "$py4_o2_bin"
    run_three_times "py4-O3" "$py4_o3_bin"
    printf '\n'
}

cleanup() {
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

cd "$ROOT_DIR"

make >/dev/null

HAS_PYPY=0
if command -v pypy3 >/dev/null 2>&1; then
    HAS_PYPY=1
fi

HAS_PYDANTIC=0
PYDANTIC_PY=".venv/bin/python"
if [ -x "$PYDANTIC_PY" ] && "$PYDANTIC_PY" -c 'import pydantic' >/dev/null 2>&1; then
    HAS_PYDANTIC=1
fi

run_benchmark "arithmetic_loop" \
    benchmarks/arithmetic_loop.py \
    benchmarks/arithmetic_loop.p4 \
    benchmarks/arithmetic_loop.go \
    benchmarks/arithmetic_loop.c

run_benchmark "list_mutation" \
    benchmarks/list_mutation.py \
    benchmarks/list_mutation.p4 \
    benchmarks/list_mutation.go \
    benchmarks/list_mutation.c

run_json_benchmark
