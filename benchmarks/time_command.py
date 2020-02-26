#!/usr/bin/env python3

import subprocess
import sys
import tempfile
import time


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: time_command.py <command> [args...]", file=sys.stderr)
        return 1

    with tempfile.NamedTemporaryFile() as rss_file:
        started = time.perf_counter()
        completed = subprocess.run(
            ["/usr/bin/time", "-f", "%M", "-o", rss_file.name, *sys.argv[1:]],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        elapsed = time.perf_counter() - started

        rss_file.seek(0)
        max_rss = rss_file.read().decode("utf-8").strip()

    print(f"{elapsed:.6f}s {max_rss}KB")
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
