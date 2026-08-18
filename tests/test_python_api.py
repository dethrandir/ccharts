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


if __name__ == "__main__":
    unittest.main()
