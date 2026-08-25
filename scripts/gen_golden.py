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


# Mirrors ccharts_pie_slice in abi/ccharts_abi.h.
class PieSlice(ctypes.Structure):
    _fields_ = [
        ("label", ctypes.c_char_p),
        ("value", ctypes.c_double),
    ]


# Mirrors ccharts_hist_settings in abi/ccharts_abi.h.
class HistSettings(ctypes.Structure):
    _fields_ = [
        ("rise_color", ctypes.c_char_p),
        ("bg_color", ctypes.c_char_p),
        ("bin_count", ctypes.c_int32),
        ("min_value", ctypes.c_double),
        ("max_value", ctypes.c_double),
        ("show_bins", ctypes.c_int32),
        ("show_prices", ctypes.c_int32),
    ]


# Mirrors ccharts_spark_settings in abi/ccharts_abi.h.
class SparkSettings(ctypes.Structure):
    _fields_ = [
        ("rise_color", ctypes.c_char_p),
        ("area_color", ctypes.c_char_p),
        ("min_above", ctypes.c_int32),
        ("min_below", ctypes.c_int32),
    ]


# Mirrors ccharts_bar_slice in abi/ccharts_abi.h.
class BarItem(ctypes.Structure):
    _fields_ = [
        ("label", ctypes.c_char_p),
        ("value", ctypes.c_double),
    ]


# Mirrors ccharts_bar_settings in abi/ccharts_abi.h.
class BarSettings(ctypes.Structure):
    _fields_ = [
        ("rise_color", ctypes.c_char_p),
        ("bg_color", ctypes.c_char_p),
        ("show_labels", ctypes.c_int32),
        ("show_prices", ctypes.c_int32),
    ]


# Mirrors ccharts_stack_series in abi/ccharts_abi.h.
class StackSeries(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("values", ctypes.POINTER(ctypes.c_double)),
    ]


# Mirrors ccharts_stack_settings in abi/ccharts_abi.h.
class StackSettings(ctypes.Structure):
    _fields_ = [
        ("colors", ctypes.POINTER(ctypes.c_char_p)),
        ("bg_color", ctypes.c_char_p),
        ("cat_labels", ctypes.POINTER(ctypes.c_char_p)),
        ("series", ctypes.c_int32),
        ("cats", ctypes.c_int32),
        ("show_labels", ctypes.c_int32),
        ("show_prices", ctypes.c_int32),
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
    lib.ccharts_pie_from_slices.restype = ctypes.c_int32
    lib.ccharts_pie_from_slices.argtypes = [
        ctypes.POINTER(PieSlice), ctypes.c_int32,
        ctypes.c_int32, ctypes.c_int32, ctypes.c_int32,
        ctypes.POINTER(ctypes.c_char_p), ctypes.c_int32,
        ctypes.c_int32, ctypes.c_int32,
        ctypes.c_double, ctypes.c_double, ctypes.c_int32,
        ctypes.c_double, ctypes.c_int32, ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
    lib.ccharts_hist.restype = ctypes.c_int32
    lib.ccharts_hist.argtypes = [
        ctypes.POINTER(ctypes.c_double), ctypes.c_int32,
        ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(HistSettings),
        ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
    lib.ccharts_spark.restype = ctypes.c_int32
    lib.ccharts_spark.argtypes = [
        ctypes.POINTER(ctypes.c_double), ctypes.c_int32,
        ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(SparkSettings),
        ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
    lib.ccharts_bar.restype = ctypes.c_int32
    lib.ccharts_bar.argtypes = [
        ctypes.POINTER(BarItem), ctypes.c_int32,
        ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(BarSettings),
        ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
    lib.ccharts_stack.restype = ctypes.c_int32
    lib.ccharts_stack.argtypes = [
        ctypes.POINTER(StackSeries), ctypes.c_int32,
        ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(StackSettings),
        ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
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


def render_pie(lib, case):
    """Renders a pie/donut case through ccharts_pie_from_slices."""
    cfg = case["settings"]
    slices = (PieSlice * len(case["slices"]))()
    for i, s in enumerate(case["slices"]):
        slices[i].label = s["label"].encode("utf-8") if s.get("label") else None
        slices[i].value = s["value"]

    color_names = cfg.get("colors")
    colors = None
    color_count = 0
    if color_names:
        colors = (ctypes.c_char_p * len(color_names))(
            *[color(lib, name) for name in color_names])
        color_count = len(color_names)

    # Optional Fas 3 pie settings. The double sentinels (-1) keep the
    # original behavior when the case leaves them unset, matching the ABI's
    # negative-means-"unspecified" convention.
    center_text = cfg.get("center_text")
    center_text_arg = center_text.encode("utf-8") if center_text else None

    out = ctypes.c_void_p()
    length = ctypes.c_size_t()
    status = lib.ccharts_pie_from_slices(
        slices, len(case["slices"]), case["width"], case["height"],
        int(cfg.get("donut", False)), colors, color_count,
        int(cfg.get("show_legend", True)), int(cfg.get("show_pct", False)),
        float(cfg.get("slice_gap", 0.0)),
        float(cfg.get("inner_radius_ratio", -1.0)),
        int(cfg.get("legend_format", 0)),
        float(cfg.get("start_angle", -1.0)),
        int(cfg.get("counter_clockwise", False)),
        center_text_arg,
        ctypes.byref(out), ctypes.byref(length))
    if status != CCHARTS_OK:
        raise SystemExit("%s: %s" % (case["name"],
                                     lib.ccharts_error_message(status).decode()))
    try:
        return ctypes.string_at(out, length.value)
    finally:
        lib.ccharts_string_free(out)


def render_hist(lib, case):
    """Renders a histogram case through ccharts_hist."""
    cfg = case["settings"]
    n = len(case["samples"])
    samples = (ctypes.c_double * n)(*case["samples"])
    plain = b"" if cfg.get("plain") else None
    settings = HistSettings(
        rise_color=plain if plain is not None else color(lib, cfg["rise_color"]),
        bg_color=plain if plain is not None else color(lib, cfg["bg_color"]),
        bin_count=int(cfg["bin_count"]),
        min_value=(float("nan") if cfg["min_value"] is None
                   else float(cfg["min_value"])),
        max_value=(float("nan") if cfg["max_value"] is None
                   else float(cfg["max_value"])),
        show_bins=int(cfg["show_bins"]),
        show_prices=int(cfg["show_prices"]),
    )
    out = ctypes.c_void_p()
    length = ctypes.c_size_t()
    status = lib.ccharts_hist(samples, n, case["width"], case["height"],
                              ctypes.byref(settings), ctypes.byref(out),
                              ctypes.byref(length))
    if status != CCHARTS_OK:
        raise SystemExit("%s: %s" % (case["name"],
                                     lib.ccharts_error_message(status).decode()))
    try:
        return ctypes.string_at(out, length.value)
    finally:
        lib.ccharts_string_free(out)


def render_spark(lib, case):
    """Renders a sparkline case through ccharts_spark."""
    cfg = case["settings"]
    n = len(case["samples"])
    samples = (ctypes.c_double * n)(*case["samples"])
    plain = b"" if cfg.get("plain") else None
    settings = SparkSettings(
        rise_color=plain if plain is not None else color(lib, cfg["rise_color"]),
        area_color=plain if plain is not None else color(lib, cfg["area_color"]),
        min_above=int(cfg.get("min_above", 0)),
        min_below=int(cfg.get("min_below", 0)),
    )
    out = ctypes.c_void_p()
    length = ctypes.c_size_t()
    status = lib.ccharts_spark(samples, n, case["width"], case["height"],
                               ctypes.byref(settings), ctypes.byref(out),
                               ctypes.byref(length))
    if status != CCHARTS_OK:
        raise SystemExit("%s: %s" % (case["name"],
                                     lib.ccharts_error_message(status).decode()))
    try:
        return ctypes.string_at(out, length.value)
    finally:
        lib.ccharts_string_free(out)


def render_bar(lib, case):
    """Renders a bar chart case through ccharts_bar."""
    cfg = case["settings"]
    n = len(case["items"])
    items = (BarItem * n)()
    for i, it in enumerate(case["items"]):
        items[i].label = it["label"].encode("utf-8") if it.get("label") else None
        items[i].value = it["value"]
    plain = b"" if cfg.get("plain") else None
    settings = BarSettings(
        rise_color=plain if plain is not None else color(lib, cfg["rise_color"]),
        bg_color=plain if plain is not None else color(lib, cfg["bg_color"]),
        show_labels=int(cfg.get("show_labels", False)),
        show_prices=int(cfg.get("show_prices", False)),
    )
    out = ctypes.c_void_p()
    length = ctypes.c_size_t()
    status = lib.ccharts_bar(items, n, case["width"], case["height"],
                             ctypes.byref(settings), ctypes.byref(out),
                             ctypes.byref(length))
    if status != CCHARTS_OK:
        raise SystemExit("%s: %s" % (case["name"],
                                     lib.ccharts_error_message(status).decode()))
    try:
        return ctypes.string_at(out, length.value)
    finally:
        lib.ccharts_string_free(out)


def render_stack(lib, case):
    """Renders a stacked bar chart case through ccharts_stack."""
    cfg = case["settings"]
    n = len(case["series"])
    vals = case["series"][0]["values"]
    cats = len(vals)
    series = (StackSeries * n)()
    values_bufs = []
    for i, s in enumerate(case["series"]):
        v = (ctypes.c_double * len(s["values"]))(*s["values"])
        values_bufs.append(v)
        series[i].name = s["name"].encode("utf-8") if s.get("name") else None
        series[i].values = v

    # The stack ABI has no `plain` flag of its own: plain rendering is forced
    # by handing the renderer a palette override where every entry is an empty
    # escape string (and an empty bg_color), exactly like the Python wrapper
    # does with plain=True. So a plain case must always get a palette override,
    # whether or not it specified colors.
    plain = cfg.get("plain")
    colors = None
    if plain:
        arr = (ctypes.c_char_p * (n + 1))(*([b""] * n))
        colors = ctypes.cast(arr, ctypes.POINTER(ctypes.c_char_p))
    elif cfg.get("colors"):
        names = cfg["colors"]
        arr = (ctypes.c_char_p * (len(names) + 1))(
            *[color(lib, name) for name in names])
        colors = ctypes.cast(arr, ctypes.POINTER(ctypes.c_char_p))
    bg_color = (b"" if plain else color(lib, cfg["bg_color"]))

    cat_labels = None
    if cfg.get("cat_labels"):
        arr = (ctypes.c_char_p * cats)(
            *[label.encode("utf-8") for label in cfg["cat_labels"]])
        cat_labels = ctypes.cast(arr, ctypes.POINTER(ctypes.c_char_p))

    settings = StackSettings(
        colors=colors,
        bg_color=bg_color,
        cat_labels=cat_labels,
        series=n,
        cats=cats,
        show_labels=int(cfg.get("show_labels", False)),
        show_prices=int(cfg.get("show_prices", False)),
    )
    out = ctypes.c_void_p()
    length = ctypes.c_size_t()
    status = lib.ccharts_stack(series, n, case["width"], case["height"],
                               ctypes.byref(settings), ctypes.byref(out),
                               ctypes.byref(length))
    if status != CCHARTS_OK:
        raise SystemExit("%s: %s" % (case["name"],
                                     lib.ccharts_error_message(status).decode()))
    try:
        return ctypes.string_at(out, length.value)
    finally:
        lib.ccharts_string_free(out)


def render(lib, case, datasets):
    if case["chart"] == "pie":
        return render_pie(lib, case)
    if case["chart"] == "hist":
        return render_hist(lib, case)
    if case["chart"] == "spark":
        return render_spark(lib, case)
    if case["chart"] == "bar":
        return render_bar(lib, case)
    if case["chart"] == "stack":
        return render_stack(lib, case)
    handle = build_data(lib, datasets[case["dataset"]], case["source"])
    try:
        cfg = case["settings"]
        # An empty C string means "emit no escape at all", which is different
        # from NULL (use the default color).
        plain = b"" if cfg.get("plain") else None
        settings = Settings(
            rise_color=plain if plain is not None else color(lib, cfg["rise_color"]),
            fall_color=plain if plain is not None else color(lib, cfg["fall_color"]),
            bg_color=plain if plain is not None else color(lib, cfg["bg_color"]),
            area_color=plain if plain is not None else color(lib, cfg["area_color"]),
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
