#!/usr/bin/env python3
"""Arrange the built native libraries into the layouts the packages expect.

The C# and Java bindings link the shared library instead of compiling the C,
so their packages carry prebuilt binaries — and each ecosystem wants them in
its own directory shape:

    NuGet   bindings/dotnet/src/Ccharts/runtimes/{rid}/native/<lib>
    Maven   bindings/java/native/{os}-{arch}/<lib>

natives.yml uploads one artifact per target, named native-{rid}, each holding
a single library file. Point this script at the directory those artifacts were
downloaded into:

    python3 scripts/pack_natives.py --artifacts dist/natives
    python3 scripts/pack_natives.py --artifacts dist/natives --check

--check verifies both layouts are complete without writing anything, which is
what a release job should run before packing.
"""

import argparse
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DOTNET_NATIVE_ROOT = os.path.join(ROOT, "bindings", "dotnet", "src", "Ccharts",
                                  "runtimes")
JAVA_NATIVE_ROOT = os.path.join(ROOT, "bindings", "java", "native")

# rid -> (library file name, the directory name Native.platformDirectory()
# builds in the Java binding).
TARGETS = {
    "linux-x64": ("libccharts_abi.so", "linux-x86_64"),
    "osx-arm64": ("libccharts_abi.dylib", "macos-aarch64"),
    "win-x64": ("ccharts_abi.dll", "windows-x86_64"),
}


def find_library(artifacts, rid, file_name):
    """Locates one target's library in the downloaded artifacts."""
    candidates = [
        os.path.join(artifacts, "native-" + rid, file_name),
        os.path.join(artifacts, rid, file_name),
        os.path.join(artifacts, file_name),
    ]
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None


def place(source, destination, check):
    if check:
        if not os.path.isfile(destination):
            return "%s: missing" % os.path.relpath(destination, ROOT)
        if os.path.getsize(destination) != os.path.getsize(source):
            return "%s: differs from the built library" % os.path.relpath(destination, ROOT)
        return None
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    shutil.copy2(source, destination)
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", required=True,
                        help="directory the native-* artifacts were downloaded into")
    parser.add_argument("--check", action="store_true",
                        help="verify the layouts instead of writing them")
    args = parser.parse_args()

    problems = []
    placed = 0

    for rid, (file_name, java_dir) in sorted(TARGETS.items()):
        source = find_library(args.artifacts, rid, file_name)
        if source is None:
            problems.append("no library for %s (expected %s/native-%s/%s)"
                            % (rid, args.artifacts, rid, file_name))
            continue

        destinations = [
            os.path.join(DOTNET_NATIVE_ROOT, rid, "native", file_name),
            os.path.join(JAVA_NATIVE_ROOT, java_dir, file_name),
        ]
        for destination in destinations:
            problem = place(source, destination, args.check)
            if problem:
                problems.append(problem)
            else:
                placed += 1

    if problems:
        for problem in problems:
            print("ERROR: %s" % problem, file=sys.stderr)
        return 1

    if args.check:
        print("native layouts complete: %d file(s) for %d target(s)"
              % (placed, len(TARGETS)))
    else:
        print("placed %d file(s) for %d target(s)" % (placed, len(TARGETS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
