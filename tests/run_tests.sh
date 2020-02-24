#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OK_DIR="$ROOT_DIR/tests/cases/ok"
FAIL_DIR="$ROOT_DIR/tests/cases/fail"
RUNTIME_FAIL_DIR="$ROOT_DIR/tests/cases/runtime_fail"
CODEGEN_DIR="$ROOT_DIR/tests/cases/codegen"
TMP_DIR="$(mktemp -d)"
GENERATED_RUNTIME_SRC="$ROOT_DIR/runtime/vendor/cjson/cJSON.c"

cleanup() {
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

pass_count=0
fail_count=0

run_cli_compile_checks() {
    local case_file expected default_bin release_bin opt3_bin clang_bin

    case_file="$ROOT_DIR/tests/cases/ok/functions_and_calls.p4"
    expected="$(cat "${case_file%.p4}.out")"
    default_bin="$TMP_DIR/cli-default-bin"

    if ! "$ROOT_DIR/py4" -o "$default_bin" "$case_file" >"$TMP_DIR/cli-default.stdout" 2>"$TMP_DIR/cli-default.stderr"; then
        printf 'FAIL cli/compile_default: py4 default compile failed\n'
        sed 's/^/    /' "$TMP_DIR/cli-default.stderr"
        fail_count=$((fail_count + 1))
        return
    fi

    if [[ "$("$default_bin")" != "$expected" ]]; then
        printf 'FAIL cli/compile_default: compiled binary output mismatch\n'
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS cli/compile_default\n'
    pass_count=$((pass_count + 1))

    release_bin="$TMP_DIR/cli-release-bin"
    if ! "$ROOT_DIR/py4" --release -o "$release_bin" "$case_file" >"$TMP_DIR/cli-release.stdout" 2>"$TMP_DIR/cli-release.stderr"; then
        printf 'FAIL cli/compile_release: py4 --release failed\n'
        sed 's/^/    /' "$TMP_DIR/cli-release.stderr"
        fail_count=$((fail_count + 1))
        return
    fi

    if [[ "$("$release_bin")" != "$expected" ]]; then
        printf 'FAIL cli/compile_release: compiled binary output mismatch\n'
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS cli/compile_release\n'
    pass_count=$((pass_count + 1))

    opt3_bin="$TMP_DIR/cli-opt3-bin"
    if ! "$ROOT_DIR/py4" --opt-level 3 -o "$opt3_bin" "$case_file" >"$TMP_DIR/cli-opt3.stdout" 2>"$TMP_DIR/cli-opt3.stderr"; then
        printf 'FAIL cli/compile_opt3: py4 --opt-level 3 failed\n'
        sed 's/^/    /' "$TMP_DIR/cli-opt3.stderr"
        fail_count=$((fail_count + 1))
        return
    fi

    if [[ "$("$opt3_bin")" != "$expected" ]]; then
        printf 'FAIL cli/compile_opt3: compiled binary output mismatch\n'
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS cli/compile_opt3\n'
    pass_count=$((pass_count + 1))

    if ! "$ROOT_DIR/py4" --emit-c "$case_file" >"$TMP_DIR/cli-emit.c" 2>"$TMP_DIR/cli-emit.stderr"; then
        printf 'FAIL cli/emit_c: py4 --emit-c failed\n'
        sed 's/^/    /' "$TMP_DIR/cli-emit.stderr"
        fail_count=$((fail_count + 1))
        return
    fi

    if ! gcc -std=c11 "$TMP_DIR/cli-emit.c" "$GENERATED_RUNTIME_SRC" -o "$TMP_DIR/cli-emit-bin" >"$TMP_DIR/cli-emit.gcc.log" 2>&1; then
        printf 'FAIL cli/emit_c: emitted C did not compile\n'
        cat "$TMP_DIR/cli-emit.gcc.log"
        fail_count=$((fail_count + 1))
        return
    fi

    if [[ "$("$TMP_DIR/cli-emit-bin")" != "$expected" ]]; then
        printf 'FAIL cli/emit_c: emitted C binary output mismatch\n'
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS cli/emit_c\n'
    pass_count=$((pass_count + 1))

    if command -v clang >/dev/null 2>&1; then
        clang_bin="$TMP_DIR/cli-clang-bin"
        if ! "$ROOT_DIR/py4" --backend clang -o "$clang_bin" "$case_file" >"$TMP_DIR/cli-clang.stdout" 2>"$TMP_DIR/cli-clang.stderr"; then
            printf 'FAIL cli/compile_clang: py4 default compile with clang failed\n'
            sed 's/^/    /' "$TMP_DIR/cli-clang.stderr"
            fail_count=$((fail_count + 1))
            return
        fi

        if [[ "$("$clang_bin")" != "$expected" ]]; then
            printf 'FAIL cli/compile_clang: compiled binary output mismatch\n'
            fail_count=$((fail_count + 1))
            return
        fi

        printf 'PASS cli/compile_clang\n'
        pass_count=$((pass_count + 1))
    fi
}

run_ok_case() {
    local case_file="$1"
    local base_name generated_c generated_bin expected actual

    base_name="$(basename "$case_file" .p4)"
    generated_c="$TMP_DIR/${base_name}.c"
    generated_bin="$TMP_DIR/${base_name}"
    expected="$(cat "${case_file%.p4}.out")"

    if ! "$ROOT_DIR/py4" --emit-c "$case_file" > "$generated_c"; then
        printf 'FAIL ok/%s: transpiler exited with failure\n' "$base_name"
        fail_count=$((fail_count + 1))
        return
    fi

    if ! gcc -std=c11 "$generated_c" "$GENERATED_RUNTIME_SRC" -o "$generated_bin" >"$TMP_DIR/${base_name}.gcc.log" 2>&1; then
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

    if "$ROOT_DIR/py4" --emit-c "$case_file" >"$TMP_DIR/${base_name}.stdout" 2>"$stderr_file"; then
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

run_runtime_fail_case() {
    local case_file="$1"
    local base_name generated_c generated_bin stderr_file expected

    base_name="$(basename "$case_file" .p4)"
    generated_c="$TMP_DIR/${base_name}.c"
    generated_bin="$TMP_DIR/${base_name}"
    stderr_file="$TMP_DIR/${base_name}.runtime.stderr"
    expected="$(cat "${case_file%.p4}.err")"

    if ! "$ROOT_DIR/py4" --emit-c "$case_file" > "$generated_c"; then
        printf 'FAIL runtime_fail/%s: transpiler exited with failure\n' "$base_name"
        fail_count=$((fail_count + 1))
        return
    fi

    if ! gcc -std=c11 "$generated_c" "$GENERATED_RUNTIME_SRC" -o "$generated_bin" >"$TMP_DIR/${base_name}.gcc.log" 2>&1; then
        printf 'FAIL runtime_fail/%s: generated C did not compile\n' "$base_name"
        cat "$TMP_DIR/${base_name}.gcc.log"
        fail_count=$((fail_count + 1))
        return
    fi

    if "$generated_bin" >"$TMP_DIR/${base_name}.runtime.stdout" 2>"$stderr_file"; then
        printf 'FAIL runtime_fail/%s: runtime succeeded unexpectedly\n' "$base_name"
        fail_count=$((fail_count + 1))
        return
    fi

    if ! grep -Fq "$expected" "$stderr_file"; then
        printf 'FAIL runtime_fail/%s: expected runtime error substring not found\n' "$base_name"
        printf '  expected substring: %q\n' "$expected"
        printf '  stderr:\n'
        sed 's/^/    /' "$stderr_file"
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS runtime_fail/%s\n' "$base_name"
    pass_count=$((pass_count + 1))
}

run_codegen_case() {
    local case_file="$1"
    local base_name generated_c generated_bin expect_file gcc_log missing=0

    base_name="$(basename "$case_file" .p4)"
    generated_c="$TMP_DIR/${base_name}.codegen.c"
    generated_bin="$TMP_DIR/${base_name}.codegen"
    expect_file="${case_file%.p4}.c.expect"
    gcc_log="$TMP_DIR/${base_name}.codegen.gcc.log"

    if ! "$ROOT_DIR/py4" --emit-c "$case_file" > "$generated_c"; then
        printf 'FAIL codegen/%s: transpiler exited with failure\n' "$base_name"
        fail_count=$((fail_count + 1))
        return
    fi

    if ! gcc -std=c11 "$generated_c" "$GENERATED_RUNTIME_SRC" -o "$generated_bin" >"$gcc_log" 2>&1; then
        printf 'FAIL codegen/%s: generated C did not compile\n' "$base_name"
        cat "$gcc_log"
        fail_count=$((fail_count + 1))
        return
    fi

    while IFS= read -r expected || [[ -n "$expected" ]]; do
        [[ -n "$expected" ]] || continue
        [[ "$expected" == \#* ]] || {
            if ! grep -Fq "$expected" "$generated_c"; then
                if [[ "$missing" -eq 0 ]]; then
                    printf 'FAIL codegen/%s: expected generated C contract not found\n' "$base_name"
                fi
                printf '  missing: %s\n' "$expected"
                missing=1
            fi
            continue
        }
    done < "$expect_file"

    if [[ "$missing" -ne 0 ]]; then
        fail_count=$((fail_count + 1))
        return
    fi

    printf 'PASS codegen/%s\n' "$base_name"
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

for case_file in "$RUNTIME_FAIL_DIR"/*.p4; do
    [[ -f "${case_file%.p4}.err" ]] || continue
    run_runtime_fail_case "$case_file"
done

for case_file in "$CODEGEN_DIR"/*.p4; do
    [[ -f "${case_file%.p4}.c.expect" ]] || continue
    run_codegen_case "$case_file"
done

run_cli_compile_checks

printf '\n%d passed, %d failed\n' "$pass_count" "$fail_count"

if [[ "$fail_count" -ne 0 ]]; then
    exit 1
fi
