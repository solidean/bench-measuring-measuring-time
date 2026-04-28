#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.11"
# ///
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent


def main():
    # build first
    import build as b
    b.build(config="debug")

    # run the benchmark
    config = "debug"
    if sys.platform == "win32":
        exe = ROOT / "build" / config / "bin" / config.title() / "bench-time.exe"
    else:
        exe = ROOT / "build" / config / "bin" / "bench-time"
    subprocess.run([str(exe), *sys.argv[1:]], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
