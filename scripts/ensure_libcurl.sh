#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR_DIR="$ROOT_DIR/runtime/vendor/libcurl"
LOCAL_PREFIX="$ROOT_DIR/build/vendor/libcurl-install"
LOCAL_BUILD_DIR="$ROOT_DIR/build/vendor/libcurl-build"
VERSION="${PY4_LIBCURL_VERSION:-8.18.0}"
ARCHIVE="curl-${VERSION}.tar.gz"
URL="https://curl.se/download/${ARCHIVE}"
TMP_DIR=""
MODE="${1:-ensure}"

cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}

has_pkg_config_libcurl() {
    command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libcurl
}

has_curl_config() {
    command -v curl-config >/dev/null 2>&1
}

local_curl_config_ready() {
    [ -x "$LOCAL_PREFIX/bin/curl-config" ]
}

vendor_tree_ready() {
    [ -f "$VENDOR_DIR/include/curl/curlver.h" ]
}

system_libcurl_shared_path() {
    if ! command -v ldconfig >/dev/null 2>&1; then
        return 1
    fi

    ldconfig -p 2>/dev/null | awk '/libcurl\.so(\.[0-9]+)* / { print $NF; exit }'
}

compiler_cmd() {
    if command -v cc >/dev/null 2>&1; then
        printf 'cc\n'
        return
    fi
    if command -v gcc >/dev/null 2>&1; then
        printf 'gcc\n'
        return
    fi
    printf 'failed to find a C compiler for libcurl detection\n' >&2
    exit 1
}

system_libcurl_linkable_with_vendored_headers() {
    local cc_cmd
    local test_c
    local test_bin
    local shared_lib

    vendor_tree_ready || return 1
    shared_lib="$(system_libcurl_shared_path)"
    [ -n "$shared_lib" ] || return 1
    cc_cmd="$(compiler_cmd)"
    test_c="$TMP_DIR/libcurl-check.c"
    test_bin="$TMP_DIR/libcurl-check"

    cat >"$test_c" <<'EOF'
#include <curl/curl.h>

int main(void)
{
    CURL *handle = curl_easy_init();
    if (handle != NULL) {
        curl_easy_cleanup(handle);
    }
    return 0;
}
EOF

    "$cc_cmd" -I"$VENDOR_DIR/include" "$test_c" "$shared_lib" -o "$test_bin" >/dev/null 2>&1
}

download_archive() {
    local output_path="$1"

    if command -v curl >/dev/null 2>&1; then
        curl -L "$URL" -o "$output_path"
        return
    fi

    if command -v wget >/dev/null 2>&1; then
        wget -O "$output_path" "$URL"
        return
    fi

    printf 'failed to download libcurl: neither curl nor wget is installed\n' >&2
    exit 1
}

download_vendor_tree() {
    mkdir -p "$ROOT_DIR/runtime/vendor"
    TMP_DIR="$(mktemp -d)"

    download_archive "$TMP_DIR/$ARCHIVE"
    tar -xzf "$TMP_DIR/$ARCHIVE" -C "$TMP_DIR"
    rm -rf "$VENDOR_DIR"
    mv "$TMP_DIR/curl-$VERSION" "$VENDOR_DIR"
}

bootstrap_local_libcurl() {
    local openssl_header="/usr/include/openssl/ssl.h"
    local configure_path="$VENDOR_DIR/configure"

    if local_curl_config_ready; then
        return
    fi

    vendor_tree_ready || download_vendor_tree

    if [ ! -x "$configure_path" ]; then
        printf 'vendored libcurl source tree is missing configure script\n' >&2
        exit 1
    fi

    if [ ! -f "$openssl_header" ] && ! has_pkg_config_libcurl; then
        printf 'failed to bootstrap local libcurl: install system libcurl or OpenSSL development headers\n' >&2
        exit 1
    fi

    mkdir -p "$LOCAL_BUILD_DIR"
    rm -rf "$LOCAL_PREFIX"

    (
        cd "$LOCAL_BUILD_DIR"
        "$configure_path" \
            --prefix="$LOCAL_PREFIX" \
            --disable-shared \
            --enable-static \
            --without-zlib \
            --without-brotli \
            --without-zstd \
            --without-libpsl \
            --without-libidn2 \
            --disable-ldap \
            --disable-ldaps \
            --with-openssl \
            >/dev/null
        make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)" >/dev/null
        make install >/dev/null
    )
}

resolve_source() {
    if has_pkg_config_libcurl; then
        printf 'pkg-config\n'
        return
    fi

    if has_curl_config; then
        printf 'curl-config\n'
        return
    fi

    if local_curl_config_ready; then
        printf 'local-curl-config\n'
        return
    fi

    TMP_DIR="$(mktemp -d)"
    if system_libcurl_linkable_with_vendored_headers; then
        printf 'system-lib-vendored-headers\n'
        return
    fi

    bootstrap_local_libcurl
    if local_curl_config_ready; then
        printf 'local-curl-config\n'
        return
    fi

    printf 'failed to resolve usable libcurl build flags\n' >&2
    exit 1
}

print_flags() {
    local source="$1"
    local flag_kind="$2"
    local output=""

    case "$source" in
        pkg-config)
            output="$(pkg-config "--$flag_kind" libcurl)"
            ;;
        curl-config)
            output="$(curl-config "--$flag_kind")"
            ;;
        local-curl-config)
            output="$("$LOCAL_PREFIX/bin/curl-config" "--$flag_kind")"
            ;;
        system-lib-vendored-headers)
            if [ "$flag_kind" = "cflags" ]; then
                output="-I$VENDOR_DIR/include"
            else
                output="$(system_libcurl_shared_path)"
            fi
            ;;
        *)
            printf 'unknown libcurl source %s\n' "$source" >&2
            exit 1
            ;;
    esac

    for flag in $output; do
        printf '%s\n' "$flag"
    done
}

trap cleanup EXIT

case "$MODE" in
    ensure)
        case "$(resolve_source)" in
            pkg-config)
                printf 'using system libcurl via pkg-config\n'
                ;;
            curl-config)
                printf 'using system libcurl via curl-config\n'
                ;;
            local-curl-config)
                printf 'using locally bootstrapped libcurl at %s\n' "$LOCAL_PREFIX"
                ;;
            system-lib-vendored-headers)
                printf 'using system libcurl with vendored headers at %s\n' "$VENDOR_DIR"
                ;;
        esac
        ;;
    --print-cflags)
        print_flags "$(resolve_source)" cflags
        ;;
    --print-libs)
        print_flags "$(resolve_source)" libs
        ;;
    *)
        printf 'usage: ensure_libcurl.sh [ensure|--print-cflags|--print-libs]\n' >&2
        exit 1
        ;;
esac
