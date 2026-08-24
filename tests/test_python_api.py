"""Tests for the ccharts Python bindings.

Run from the repo root (or via `make test-py`):

    python3 -m unittest tests.test_python_api -v
"""

import array
import gc
import json
import unittest
from datetime import datetime

try:
    from ccharts import Chart
except ImportError:  # checkout without an in-place build, or cwd shadowing
    import importlib.machinery
    import os
    import sys

    root = os.path.join(os.path.dirname(__file__), "..")
    built_in_place = any(
        os.path.exists(os.path.join(root, "ccharts", name))
        for name in importlib.machinery.EXTENSION_SUFFIXES
    )
    if built_in_place:
        # Local in-place build (`setup.py build_ext --inplace`): the checkout
        # itself is the package under test.
        sys.path.insert(0, root)
        from ccharts import Chart
    else:
        # Wheel-test context (CI): the repo checkout on sys.path (cwd, in
        # relative or absolute form) shadows an installed wheel without a
        # compiled extension. Drop every cwd entry and import the installed
        # package instead.
        cwd_entries = {"", os.curdir, os.getcwd()}
        sys.path[:] = [p for p in sys.path if p not in cwd_entries]
        from ccharts import Chart

SAMPLE_JSON = """[
  {"ts": "2026-07-20T00:00:00+00:00", "open": 328.75, "high": 330.0, "low": 323.75, "close": 328.0, "volume": 100},
  {"ts": "2026-07-21T00:00:00+00:00", "open": 330.0, "high": 330.25, "low": 317.5, "close": 317.5, "volume": 200},
  {"ts": "2026-07-22T00:00:00+00:00", "open": 317.25, "high": 321.0, "low": 314.5, "close": 321.0, "volume": 300},
  {"ts": "2026-07-23T00:00:00+00:00", "open": 320.0, "high": 328.75, "low": 317.75, "close": 318.0, "volume": 400},
  {"ts": "2026-07-24T00:00:00+00:00", "open": 306.0, "high": 307.25, "low": 300.75, "close": 301.0, "volume": 500}
]"""

SAMPLE_NO_TS_JSON = """[
  {"open": 1, "high": 2, "low": 0.5, "close": 1.5},
  {"open": 1.5, "high": 2.5, "low": 1, "close": 1.2},
  {"open": 1.2, "high": 3, "low": 1.1, "close": 2.8}
]"""


class TestCCharts(unittest.TestCase):
    def setUp(self):
        self.chart = Chart(SAMPLE_JSON)

    def test_line_returns_string(self):
        out = self.chart.line()
        self.assertIsInstance(out, str)
        self.assertGreater(len(out), 0)
        self.assertIn("\n", out)

    def test_candle_returns_string(self):
        out = self.chart.candle()
        self.assertIsInstance(out, str)
        self.assertGreater(len(out), 0)
        self.assertIn("\n", out)

    def test_line_uses_smooth_blocks(self):
        out = self.chart.line(width=40, height=4)
        self.assertTrue(any(ch in out for ch in "▁▂▃▅▆▇█"),
                        "line should use eighth-block characters")

    def test_candle_uses_wick(self):
        out = self.chart.candle(width=20, height=5)
        self.assertIn("│", out, "candle chart should contain the thin wick character")

    def test_show_prices_line(self):
        out = self.chart.line(show_prices=True)
        self.assertIn("328.00", out)  # max close
        self.assertIn("301.00", out)  # min close

    def test_show_prices_candle(self):
        out = self.chart.candle(show_prices=True)
        self.assertIn("330.25", out)  # max high
        self.assertIn("300.75", out)  # min low

    def test_show_times_date_format(self):
        out = self.chart.line(show_times=True)
        self.assertIn("2026-07-20", out)
        self.assertIn("2026-07-24", out)

    def test_segment_colors(self):
        out = self.chart.line(rise_color="\x1b[34m", fall_color="\x1b[31m")
        self.assertIn("\x1b[34m", out)  # rise
        self.assertIn("\x1b[31m", out)  # fall

    def test_single_color_flag(self):
        out = self.chart.line(single_color=True, rise_color="\x1b[34m", fall_color="\x1b[31m")
        self.assertIn("\x1b[31m", out)  # overall fall -> single red
        self.assertNotIn("\x1b[34m", out)

    def test_area_color(self):
        out = self.chart.line(area_color="\x1b[33m")
        self.assertIn("\x1b[33m", out)

    def test_no_timestamp_is_safe(self):
        c = Chart(SAMPLE_NO_TS_JSON)
        out = c.line(show_times=True)
        self.assertGreater(len(out), 0)
        self.assertNotIn("2026", out)

    def test_invalid_json_raises(self):
        with self.assertRaises(ValueError):
            Chart("this is not json")

    def test_json_chart_can_be_destroyed(self):
        # Constructing from JSON owns a C cc_ohlc_t array behind a PyCapsule;
        # dropping the last reference must run its destructor without error.
        c = Chart(SAMPLE_JSON)
        self.assertGreater(len(c.line(width=20, height=3)), 0)
        del c
        gc.collect()

    def test_different_dimensions(self):
        out = self.chart.line(width=20, height=3)
        lines = out.strip("\n").split("\n")
        self.assertEqual(len(lines), 3)

    def test_height_one_line_is_single_row(self):
        # P5 smoke (TUI watchlist mini cells): height=1 renders one line.
        out = self.chart.line(width=12, height=1)
        lines = out.strip("\n").split("\n")
        self.assertEqual(len(lines), 1)
        self.assertGreater(len(lines[0]), 0)


class TestPlainOutput(unittest.TestCase):
    """plain=True must produce text with no escape bytes at all."""

    def setUp(self):
        self.chart = Chart(SAMPLE_JSON)

    def test_line_has_no_escapes(self):
        out = self.chart.line(width=40, height=5, show_prices=True,
                              show_times=True, plain=True)
        self.assertNotIn("\x1b", out)
        self.assertIn("2026-07-20", out)

    def test_candle_has_no_escapes(self):
        out = self.chart.candle(width=40, height=5, plain=True)
        self.assertNotIn("\x1b", out)
        self.assertIn("\u2502", out)  # wicks are still drawn

    def test_plain_overrides_explicit_colors(self):
        out = self.chart.line(width=40, height=5, plain=True,
                              rise_color="\x1b[34m", area_color="\x1b[90m")
        self.assertNotIn("\x1b", out)

    def test_colored_output_still_has_escapes(self):
        self.assertIn("\x1b", self.chart.line(width=40, height=5))


class TestInputValidation(unittest.TestCase):
    """Dimension and payload guards added during the hardening pass."""

    def setUp(self):
        self.chart = Chart(SAMPLE_JSON)

    def test_zero_width_line_raises(self):
        with self.assertRaises(ValueError):
            self.chart.line(width=0)

    def test_negative_width_line_raises(self):
        with self.assertRaises(ValueError):
            self.chart.line(width=-5)

    def test_zero_height_candle_raises(self):
        with self.assertRaises(ValueError):
            self.chart.candle(height=0)

    def test_negative_height_line_raises(self):
        with self.assertRaises(ValueError):
            self.chart.line(height=-2)

    def test_huge_dimensions_raise(self):
        # width beyond CC_MAX_DIM (100000) must fail cleanly, not segfault.
        with self.assertRaises(ValueError):
            self.chart.line(width=200000, height=10)
        with self.assertRaises(ValueError):
            self.chart.candle(width=1000, height=2000)  # > CC_MAX_CELLS

    def test_non_integer_dimensions_raise(self):
        with self.assertRaises(TypeError):
            self.chart.line(width="wide")

    def test_empty_json_array_raises(self):
        with self.assertRaises(ValueError):
            Chart("[]")

    def test_object_not_array_raises(self):
        with self.assertRaises(ValueError):
            Chart('{"open": 1}')

    def test_non_string_input_raises(self):
        with self.assertRaises(TypeError):
            Chart(42)

    def test_keyword_substring_not_false_positive(self):
        # A string value containing a key name ("opened" contains "open")
        # must not be mistaken for the open field or break parsing.
        tricky = ('[{"ts": "opened", "open": 1, "high": 2, "low": 0.5,'
                  ' "close": 1.5, "note": "a high low open close", "volume": 7}]')
        c = Chart(tricky)
        out = c.line(show_times=True)
        self.assertGreater(len(out), 0)
        self.assertNotIn("1970", out)  # bogus ts must not parse to epoch 0

    def test_json_overflow_to_infinity_raises(self):
        # "1e999" overflows atof to +inf on the JSON path; inf must be
        # rejected before it can reach the renderer's lround().
        bad = SAMPLE_JSON.replace("301.0", "1e999")
        with self.assertRaises(ValueError):
            Chart(bad)

    def test_arrays_overflow_to_infinity_raises(self):
        # 1e999 is +inf in Python too; the array path must already reject it.
        with self.assertRaises(ValueError):
            Chart.from_arrays([1.0], [2.0], [0.5], [1e999])


class TestTimezones(unittest.TestCase):
    """ISO8601 offsets must shift the stored instant (wall time -> UTC)."""

    def test_offset_changes_footer_date(self):
        # 2026-07-20T00:00:00+03:00 is 2026-07-19T21:00:00 UTC, so a daily
        # chart labeled with +03:00 shows the previous day in the footer.
        shifted = SAMPLE_JSON.replace("+00:00", "+03:00")
        out_utc = Chart(SAMPLE_JSON).line(show_times=True)
        out_plus3 = Chart(shifted).line(show_times=True)

        self.assertIn("2026-07-20", out_utc)
        self.assertIn("2026-07-19", out_plus3)
        self.assertNotIn("2026-07-20", out_plus3)
        self.assertNotEqual(out_utc, out_plus3)

    def test_negative_offset_shifts_forward(self):
        # 2026-07-20T22:00:00-05:00 is 2026-07-21T03:00:00 UTC, so a late
        # evening local time labeled -05:00 rolls into the NEXT day.
        evening = SAMPLE_JSON.replace("T00:00:00", "T22:00:00")
        shifted = evening.replace("+00:00", "-05:00")
        out_utc = Chart(evening).line(show_times=True)
        out_minus5 = Chart(shifted).line(show_times=True)
        self.assertIn("2026-07-20", out_utc)
        self.assertIn("2026-07-21", out_minus5)
        self.assertNotEqual(out_utc, out_minus5)

    def test_z_suffix_equals_utc(self):
        shifted = SAMPLE_JSON.replace("+00:00", "Z")
        out_z = Chart(shifted).line(show_times=True)
        self.assertIn("2026-07-20", out_z)
        self.assertIn("2026-07-24", out_z)


class TestDownsampling(unittest.TestCase):
    """Many candles into a narrow width must still render on both charts."""

    def _many_candles(self, n=200):
        rows = ",".join(
            '{"ts": %d, "open": %d, "high": %d, "low": %d, "close": %d}'
            % (i, i, i + 1, i - 1, i)
            for i in range(n)
        )
        return Chart("[" + rows + "]")

    def test_line_downsampling(self):
        c = self._many_candles(200)
        out = c.line(width=10, height=3)
        lines = out.strip("\n").split("\n")
        self.assertEqual(len(lines), 3)
        self.assertGreater(len(out), 0)

    def test_candle_downsampling(self):
        c = self._many_candles(200)
        out = c.candle(width=10, height=3)
        self.assertGreater(len(out), 0)
        self.assertIn("\n", out)

    def test_single_candle(self):
        c = Chart('[{"open": 1, "high": 2, "low": 0.5, "close": 1.5}]')
        self.assertGreater(len(c.line(width=10, height=3)), 0)
        self.assertGreater(len(c.candle(width=10, height=3)), 0)


class TestFromArrays(unittest.TestCase):
    """Columnar input path (ccharts._core.parse_arrays), no pandas involved."""

    def setUp(self):
        rows = json.loads(SAMPLE_JSON)
        self.rows = rows
        self.open = [r["open"] for r in rows]
        self.high = [r["high"] for r in rows]
        self.low = [r["low"] for r in rows]
        self.close = [r["close"] for r in rows]
        self.ts = [
            int(datetime.strptime(r["ts"], "%Y-%m-%dT%H:%M:%S%z").timestamp())
            for r in rows
        ]

    def _from_lists(self, ts=None):
        return Chart.from_arrays(self.open, self.high, self.low, self.close, ts=ts)

    def test_lists_render(self):
        out = self._from_lists().line(width=40, height=4)
        self.assertIsInstance(out, str)
        self.assertGreater(len(out), 0)

    def test_matches_json_path_exactly(self):
        # The new entry point must not change a single byte of the render.
        json_chart = Chart(SAMPLE_JSON)
        array_chart = self._from_lists(ts=self.ts)
        for kwargs in ({}, {"show_prices": True, "show_times": True},
                       {"width": 13, "height": 2}):
            for method in ("line", "candle"):
                self.assertEqual(getattr(json_chart, method)(**kwargs),
                                 getattr(array_chart, method)(**kwargs),
                                 "%s%r differs between the JSON and array paths"
                                 % (method, kwargs))

    def test_buffer_input(self):
        # array.array('d') exercises the memcpy fast path, 'q' the ts one.
        chart = Chart.from_arrays(
            array.array("d", self.open), array.array("d", self.high),
            array.array("d", self.low), array.array("d", self.close),
            ts=array.array("q", self.ts),
        )
        self.assertEqual(chart.line(show_times=True),
                         Chart(SAMPLE_JSON).line(show_times=True))

    def test_integer_input(self):
        # Plain ints must convert; they are not float64 buffers.
        chart = Chart.from_arrays([1, 2, 3], [2, 3, 4], [0, 1, 2], [2, 1, 3])
        self.assertGreater(len(chart.candle(width=10, height=3)), 0)

    def test_without_ts_has_no_footer(self):
        out = self._from_lists().line(width=20, height=3, show_times=True)
        self.assertEqual(len(out.strip("\n").split("\n")), 3)
        self.assertNotIn("2026", out)

    def test_length_mismatch_raises(self):
        with self.assertRaises(ValueError):
            Chart.from_arrays([1.0, 2.0], [2.0], [0.5, 1.0], [1.5, 2.0])

    def test_ts_length_mismatch_raises(self):
        with self.assertRaises(ValueError):
            Chart.from_arrays([1.0], [2.0], [0.5], [1.5], ts=[1, 2])

    def test_empty_raises(self):
        with self.assertRaises(ValueError):
            Chart.from_arrays([], [], [], [])

    def test_non_finite_raises(self):
        for bad in (float("nan"), float("inf"), float("-inf")):
            with self.assertRaises(ValueError):
                Chart.from_arrays([bad], [2.0], [0.5], [1.5])

    def test_non_numeric_raises(self):
        with self.assertRaises(TypeError):
            Chart.from_arrays(["a"], ["b"], ["c"], ["d"])


if __name__ == "__main__":
    unittest.main()