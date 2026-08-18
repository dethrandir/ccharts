"""Tests for the ccharts Python bindings.

Run from the repo root (or via `make test-py`):

    python3 -m unittest tests.test_python_api -v
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

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

    def test_different_dimensions(self):
        out = self.chart.line(width=20, height=3)
        lines = out.strip("\n").split("\n")
        self.assertEqual(len(lines), 3)


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


if __name__ == "__main__":
    unittest.main()