#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OK_DIR="$ROOT_DIR/tests/cases/ok"
FAIL_DIR="$ROOT_DIR/tests/cases/fail"
TMP_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

pass_count=0
fail_count=0

run_ok_case() {
    local case_file="$1"
    local base_name generated_c generated_bin expected actual

    base_name="$(basename "$case_file" .p4)"
    generated_c="$TMP_DIR/${base_name}.c"
    generated_bin="$TMP_DIR/${base_name}"
    expected="$(cat "${case_file%.p4}.out")"

    if ! "$ROOT_DIR/py4" "$case_file" > "$generated_c"; then
        printf 'FAIL ok/%s: transpiler exited with failure\n' "$base_name"
        fail_count=$((fail_count + 1))
        return
    fi

    if ! gcc -std=c11 "$generated_c" -o "$generated_bin" >"$TMP_DIR/${base_name}.gcc.log" 2>&1; then
        printf 'FAIL ok/%s: generated C did not compile\n' "$base_name"
        cat "$TMP_DIR/${base_name}.gcc.log"
        fail_count=$((fail_count + 1))
        return
    fi

    actual="$("$generated_bin")"
    if [[ "$actual" != "$expected" ]]; then
        printf 'FAIL ok/%s: runtime output mismatch\n' "$base_name"
        printf '  expected: %q\n' "$expected"
        printf '  actual:   %q\n' "$actual"
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS ok/%s\n' "$base_name"
    pass_count=$((pass_count + 1))
}

run_fail_case() {
    local case_file="$1"
    local base_name stderr_file expected

    base_name="$(basename "$case_file" .p4)"
    stderr_file="$TMP_DIR/${base_name}.stderr"
    expected="$(cat "${case_file%.p4}.err")"

    if "$ROOT_DIR/py4" "$case_file" >"$TMP_DIR/${base_name}.stdout" 2>"$stderr_file"; then
        printf 'FAIL fail/%s: transpiler succeeded unexpectedly\n' "$base_name"
        fail_count=$((fail_count + 1))
        return
    fi

    if ! grep -Fq "$expected" "$stderr_file"; then
        printf 'FAIL fail/%s: expected error substring not found\n' "$base_name"
        printf '  expected substring: %q\n' "$expected"
        printf '  stderr:\n'
        sed 's/^/    /' "$stderr_file"
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS fail/%s\n' "$base_name"
    pass_count=$((pass_count + 1))
}

for case_file in "$OK_DIR"/*.p4; do
    [[ -f "${case_file%.p4}.out" ]] || continue
    run_ok_case "$case_file"
done

for case_file in "$FAIL_DIR"/*.p4; do
    [[ -f "${case_file%.p4}.err" ]] || continue
    run_fail_case "$case_file"
done

printf '\n%d passed, %d failed\n' "$pass_count" "$fail_count"

if [[ "$fail_count" -ne 0 ]]; then
    exit 1
fi
