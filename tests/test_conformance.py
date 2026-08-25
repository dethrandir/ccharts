"""Runs the shared conformance suite through the Python binding.

conformance/cases.json and conformance/golden/*.txt are the cross-language
contract: every binding renders the same cases and must produce the same
bytes. The goldens are generated from the C ABI (scripts/gen_golden.py), so
this file is what proves the Python path and the ABI path agree — and it is
the template each new binding's test suite follows.

Run from the repo root (or via `make test-py`):

    python3 -m unittest tests.test_conformance -v
"""

import json
import os
import unittest

try:
    from ccharts import Chart
except ImportError:  # checkout without an in-place build, or cwd shadowing
    import importlib.machinery
    import sys

    root = os.path.join(os.path.dirname(__file__), "..")
    built_in_place = any(
        os.path.exists(os.path.join(root, "ccharts", name))
        for name in importlib.machinery.EXTENSION_SUFFIXES
    )
    if built_in_place:
        sys.path.insert(0, root)
        from ccharts import Chart
    else:
        cwd_entries = {"", os.curdir, os.getcwd()}
        sys.path[:] = [p for p in sys.path if p not in cwd_entries]
        from ccharts import Chart

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CASES = os.path.join(ROOT, "conformance", "cases.json")
GOLDEN_DIR = os.path.join(ROOT, "conformance", "golden")

# Mirrors ccharts_color() / ccharts_color_index in abi/ccharts_abi.h. The
# Python API takes raw escape strings, so the names used in cases.json are
# resolved here.
COLORS = {
    "black": "\x1b[30m", "red": "\x1b[31m", "green": "\x1b[32m",
    "yellow": "\x1b[33m", "blue": "\x1b[34m", "magenta": "\x1b[35m",
    "cyan": "\x1b[36m", "white": "\x1b[37m",
    "bright_black": "\x1b[90m", "bright_red": "\x1b[91m",
    "bright_green": "\x1b[92m", "bright_yellow": "\x1b[93m",
    "bright_blue": "\x1b[94m", "bright_magenta": "\x1b[95m",
    "bright_cyan": "\x1b[96m", "bright_white": "\x1b[97m",
    "reset": "\x1b[0m",
}


def load_suite():
    if not os.path.exists(CASES):
        return None
    with open(CASES, encoding="utf-8") as f:
        return json.load(f)


SUITE = load_suite()


@unittest.skipUnless(SUITE, "conformance/cases.json is missing")
class TestConformance(unittest.TestCase):
    """Byte-for-byte agreement with the goldens produced by the C ABI."""

    def _chart(self, case):
        dataset = SUITE["datasets"][case["dataset"]]
        if case["source"] == "json":
            return Chart(dataset["json"])
        return Chart.from_arrays(dataset["open"], dataset["high"],
                                 dataset["low"], dataset["close"],
                                 ts=dataset.get("ts"))

    def _render(self, case):
        cfg = case["settings"]
        if case["chart"] == "pie":
            return Chart.pie(
                [s["label"] for s in case["slices"]],
                [s["value"] for s in case["slices"]],
                width=case["width"], height=case["height"],
                donut=cfg.get("donut", False),
                colors=[COLORS[c] for c in cfg["colors"]]
                if cfg.get("colors") else None,
                show_legend=cfg.get("show_legend", True),
                show_pct=cfg.get("show_pct", False),
                slice_gap=cfg.get("slice_gap", 0.0),
                inner_radius_ratio=cfg.get("inner_radius_ratio"),
                legend_format=cfg.get("legend_format", 0),
                start_angle=cfg.get("start_angle"),
                counter_clockwise=cfg.get("counter_clockwise", False),
                center_text=cfg.get("center_text"),
            )
        if case["chart"] == "hist":
            return Chart.histogram(
                case["samples"],
                width=case["width"], height=case["height"],
                bin_count=cfg.get("bin_count", 0),
                min_value=cfg.get("min_value"),
                max_value=cfg.get("max_value"),
                rise_color=COLORS.get(cfg["rise_color"]),
                bg_color=COLORS.get(cfg["bg_color"]),
                show_bins=cfg.get("show_bins", False),
                show_prices=cfg.get("show_prices", False),
                plain=cfg.get("plain", False),
            )
        if case["chart"] == "spark":
            return Chart.sparkline(
                case["samples"],
                width=case["width"], height=case["height"],
                rise_color=COLORS.get(cfg["rise_color"]),
                area_color=COLORS.get(cfg["area_color"]),
                min_above=cfg.get("min_above", 0),
                min_below=cfg.get("min_below", 0),
                plain=cfg.get("plain", False),
            )
        if case["chart"] == "bar":
            return Chart.bar(
                [it["label"] for it in case["items"]],
                [it["value"] for it in case["items"]],
                width=case["width"], height=case["height"],
                color=COLORS.get(cfg["rise_color"]),
                bg_color=COLORS.get(cfg["bg_color"]),
                show_labels=cfg.get("show_labels", False),
                show_prices=cfg.get("show_prices", False),
                plain=cfg.get("plain", False),
            )
        draw = getattr(self._chart(case), case["chart"])
        return draw(
            width=case["width"], height=case["height"],
            rise_color=COLORS.get(cfg["rise_color"]),
            fall_color=COLORS.get(cfg["fall_color"]),
            bg_color=COLORS.get(cfg["bg_color"]),
            area_color=COLORS.get(cfg["area_color"]),
            single_color=cfg["single_color"],
            show_prices=cfg["show_prices"],
            show_times=cfg["show_times"],
            plain=cfg.get("plain", False),
        )

    def test_every_case_matches_its_golden(self):
        missing, differing = [], []
        for case in SUITE["cases"]:
            path = os.path.join(GOLDEN_DIR, case["name"] + ".txt")
            if not os.path.exists(path):
                missing.append(case["name"])
                continue
            with open(path, "rb") as f:
                expected = f.read()
            if self._render(case).encode("utf-8") != expected:
                differing.append(case["name"])

        self.assertEqual([], missing,
                         "golden files missing; run scripts/gen_golden.py")
        self.assertEqual([], differing,
                         "the Python binding disagrees with the C ABI goldens")

    def test_suite_is_not_empty(self):
        # Guards against a truncated cases.json quietly passing everything.
        self.assertGreaterEqual(len(SUITE["cases"]), 10)
        self.assertTrue(SUITE["datasets"])
