#!/usr/bin/env python3
"""Generate (or verify) the conformance golden files from the C ABI.

The renders come from the *shared* library through ctypes, which makes this
script double as a test of the exported ABI itself — the same path C# and Java
will take. Every other binding is then checked against these bytes.

    python3 scripts/gen_golden.py             # write conformance/golden/*.txt
    python3 scripts/gen_golden.py --check     # fail if anything drifted
    python3 scripts/gen_golden.py --lib PATH  # explicit library location

CI runs --check, so an unintended change to the renderer breaks the build
instead of silently rewriting the expectations.
"""

import argparse
import ctypes
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = os.path.join(ROOT, "conformance", "cases.json")
GOLDEN_DIR = os.path.join(ROOT, "conformance", "golden")

CCHARTS_OK = 0

# Must match ccharts_color_index in abi/ccharts_abi.h.
COLOR_NAMES = [
    "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
    "bright_black", "bright_red", "bright_green", "bright_yellow",
    "bright_blue", "bright_magenta", "bright_cyan", "bright_white", "reset",
]


class Settings(ctypes.Structure):
    _fields_ = [
        ("rise_color", ctypes.c_char_p),
        ("fall_color", ctypes.c_char_p),
        ("bg_color", ctypes.c_char_p),
        ("area_color", ctypes.c_char_p),
        ("single_color", ctypes.c_int32),
        ("show_prices", ctypes.c_int32),
        ("show_times", ctypes.c_int32),
    ]


def find_library(explicit=None):
    if explicit:
        return explicit
    names = ["libccharts_abi.so", "libccharts_abi.dylib", "ccharts_abi.dll",
             "libccharts_abi.dll"]
    roots = [os.path.join(ROOT, "build"), os.path.join(ROOT, "build", "Release"),
             os.environ.get("CCHARTS_BUILD_DIR", "")]
    for root in roots:
        if not root:
            continue
        for name in names:
            path = os.path.join(root, name)
            if os.path.exists(path):
                return path
    raise SystemExit(
        "shared library not found; build it first:\n"
        "  cmake -S . -B build && cmake --build build\n"
        "or pass --lib PATH")


def load(path):
    lib = ctypes.CDLL(path)
    lib.ccharts_from_arrays.restype = ctypes.c_int32
    lib.ccharts_from_arrays.argtypes = [
        ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
        ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
        ctypes.POINTER(ctypes.c_int64), ctypes.c_int32,
        ctypes.POINTER(ctypes.c_void_p)]
    lib.ccharts_parse_json.restype = ctypes.c_int32
    lib.ccharts_parse_json.argtypes = [ctypes.c_char_p,
                                       ctypes.POINTER(ctypes.c_void_p)]
    lib.ccharts_data_len.restype = ctypes.c_int32
    lib.ccharts_data_len.argtypes = [ctypes.c_void_p]
    lib.ccharts_data_free.restype = None
    lib.ccharts_data_free.argtypes = [ctypes.c_void_p]
    for name in ("ccharts_line", "ccharts_candle"):
        fn = getattr(lib, name)
        fn.restype = ctypes.c_int32
        fn.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32,
                       ctypes.POINTER(Settings), ctypes.POINTER(ctypes.c_void_p),
                       ctypes.POINTER(ctypes.c_size_t)]
    lib.ccharts_string_free.restype = None
    lib.ccharts_string_free.argtypes = [ctypes.c_void_p]
    lib.ccharts_color.restype = ctypes.c_char_p
    lib.ccharts_color.argtypes = [ctypes.c_int32]
    lib.ccharts_error_message.restype = ctypes.c_char_p
    lib.ccharts_error_message.argtypes = [ctypes.c_int32]
    lib.ccharts_version.restype = ctypes.c_char_p
    return lib


def build_data(lib, dataset, source):
    handle = ctypes.c_void_p()
    if source == "json":
        status = lib.ccharts_parse_json(dataset["json"].encode("utf-8"),
                                        ctypes.byref(handle))
    else:
        n = len(dataset["open"])
        cols = [(ctypes.c_double * n)(*dataset[k])
                for k in ("open", "high", "low", "close")]
        ts = dataset.get("ts")
        ts_arg = (ctypes.c_int64 * n)(*ts) if ts else None
        status = lib.ccharts_from_arrays(cols[0], cols[1], cols[2], cols[3],
                                         ts_arg, n, ctypes.byref(handle))
    if status != CCHARTS_OK:
        raise SystemExit("building the dataset failed: %s"
                         % lib.ccharts_error_message(status).decode())
    return handle


def color(lib, name):
    if name is None:
        return None
    if name not in COLOR_NAMES:
        raise SystemExit("unknown color name in cases.json: %r" % name)
    return lib.ccharts_color(COLOR_NAMES.index(name))


def render(lib, case, datasets):
    handle = build_data(lib, datasets[case["dataset"]], case["source"])
    try:
        cfg = case["settings"]
        settings = Settings(
            rise_color=color(lib, cfg["rise_color"]),
            fall_color=color(lib, cfg["fall_color"]),
            bg_color=color(lib, cfg["bg_color"]),
            area_color=color(lib, cfg["area_color"]),
            single_color=int(cfg["single_color"]),
            show_prices=int(cfg["show_prices"]),
            show_times=int(cfg["show_times"]),
        )
        out = ctypes.c_void_p()
        length = ctypes.c_size_t()
        fn = lib.ccharts_line if case["chart"] == "line" else lib.ccharts_candle
        status = fn(handle, case["width"], case["height"], ctypes.byref(settings),
                    ctypes.byref(out), ctypes.byref(length))
        if status != CCHARTS_OK:
            raise SystemExit("%s: %s" % (case["name"],
                                         lib.ccharts_error_message(status).decode()))
        try:
            return ctypes.string_at(out, length.value)
        finally:
            lib.ccharts_string_free(out)
    finally:
        lib.ccharts_data_free(handle)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="compare instead of writing; exit 1 on any difference")
    ap.add_argument("--lib", help="path to the shared library")
    args = ap.parse_args()

    lib = load(find_library(args.lib))
    with open(CASES, encoding="utf-8") as f:
        doc = json.load(f)

    os.makedirs(GOLDEN_DIR, exist_ok=True)
    drifted, written = [], 0
    seen = set()

    for case in doc["cases"]:
        if case["name"] in seen:
            raise SystemExit("duplicate case name: %s" % case["name"])
        seen.add(case["name"])
        rendered = render(lib, case, doc["datasets"])
        path = os.path.join(GOLDEN_DIR, case["name"] + ".txt")
        if args.check:
            if not os.path.exists(path):
                drifted.append("%s: no golden file" % case["name"])
            elif open(path, "rb").read() != rendered:
                drifted.append("%s: output differs from the golden file"
                               % case["name"])
        else:
            with open(path, "wb") as f:
                f.write(rendered)
            written += 1

    stale = sorted(set(os.listdir(GOLDEN_DIR)) - {n + ".txt" for n in seen})
    if stale:
        drifted.append("stale golden files: %s" % ", ".join(stale))

    if args.check:
        if drifted:
            for line in drifted:
                print("DRIFT: %s" % line, file=sys.stderr)
            return 1
        print("golden: %d case(s) match (ccharts %s)"
              % (len(seen), lib.ccharts_version().decode()))
        return 0

    print("golden: wrote %d file(s)" % written)
    return 0


if __name__ == "__main__":
    sys.exit(main())
