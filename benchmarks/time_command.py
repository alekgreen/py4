#!/usr/bin/env python3

import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: time_command.py <command> [args...]", file=sys.stderr)
        return 1

    started = time.perf_counter()
    completed = subprocess.run(sys.argv[1:], stdout=subprocess.DEVNULL, check=False)
    elapsed = time.perf_counter() - started
    print(f"{elapsed:.6f}")
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
