#!/usr/bin/env python3
"""Assert that every packaging manifest carries the same ccharts version.

The version is duplicated by necessity — each ecosystem keeps it in its own
manifest — so it is checked mechanically instead of by discipline. New
bindings add one entry to MANIFESTS as they land.

    python3 scripts/check_versions.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# path -> regex whose first group is the version.
MANIFESTS = {
    "pyproject.toml": r'^version\s*=\s*"([^"]+)"',
    "abi/ccharts_abi.h": r'^#define CCHARTS_VERSION "([^"]+)"',
    # F2+: bindings/rust/Cargo.toml, bindings/js/package.json,
    # bindings/dotnet/Ccharts.csproj, bindings/java/pom.xml
}

REFERENCE = "pyproject.toml"


def version_in(path, pattern):
    full = os.path.join(ROOT, path)
    if not os.path.exists(full):
        return None
    with open(full, encoding="utf-8") as f:
        for line in f:
            m = re.match(pattern, line.strip())
            if m:
                return m.group(1)
    raise SystemExit("no version found in %s" % path)


def main():
    versions = {path: version_in(path, pattern)
                for path, pattern in MANIFESTS.items()}
    versions = {p: v for p, v in versions.items() if v is not None}

    expected = versions.get(REFERENCE)
    if expected is None:
        raise SystemExit("%s is missing" % REFERENCE)

    mismatched = {p: v for p, v in versions.items() if v != expected}
    if mismatched:
        print("version mismatch (expected %s from %s):" % (expected, REFERENCE),
              file=sys.stderr)
        for path, found in sorted(mismatched.items()):
            print("  %s: %s" % (path, found), file=sys.stderr)
        return 1

    print("version %s consistent across %d manifest(s)" % (expected, len(versions)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
