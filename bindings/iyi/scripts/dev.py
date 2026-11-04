#!/usr/bin/env python3
"""Build the C ABI explicitly, then exercise the iyi binding.

No install-time hooks: consumers build the shipped C sources once and point
IYI_LIBRARY_PATH at the archive. This helper uses the same steps in local QA
and CI, and never downloads data or invokes fl.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

BINDING = Path(__file__).resolve().parents[1]
ROOT = BINDING.parents[1]
BUILD = BINDING / "build"


def run(args, env):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["native", "test", "cli"])
    args = parser.parse_args()
    env = os.environ.copy()
    existing = env.get("IYI_LIBRARY_PATH")
    env["IYI_LIBRARY_PATH"] = str(BUILD) + (os.pathsep + existing if existing else "")
    run(["cmake", "-S", BINDING, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Release"], env)
    run(["cmake", "--build", BUILD, "--config", "Release"], env)
    if args.command == "test":
        run(["iyi", "tool", "format", "--check", "ccharts.iyi", "bindings/iyi"], env)
        run(["iyi", "test", "bindings/iyi/test"], env)
    if args.command in ("test", "cli"):
        binary = BUILD / ("ccharts-market.exe" if os.name == "nt" else "ccharts-market")
        run(["iyi", "build", "bindings/iyi/examples/market.iyi", "-o", binary], env)
        run([binary, "--help"], env)
        run(["iyi", "run", "bindings/iyi/examples/quickstart.iyi"], env)
        if args.command == "test":
            run([sys.executable, BINDING / "examples" / "test_market.py", binary], env)
            run([sys.executable, BINDING / "scripts" / "test_package.py"], env)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
