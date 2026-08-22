"""Tests for Chart.from_dataframe (optional pandas support).

The whole module skips when pandas is missing, so `pip install ccharts`
without the extra — and the cibuildwheel wheel tests, which never install
pandas — still run green. The C entry point these tests exercise is covered
without pandas by TestFromArrays in tests/test_python_api.py.

Run from the repo root (or via `make test-py`):

    python3 -m unittest tests.test_pandas_api -v
"""

import unittest

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
        sys.path.insert(0, root)
        from ccharts import Chart
    else:
        cwd_entries = {"", os.curdir, os.getcwd()}
        sys.path[:] = [p for p in sys.path if p not in cwd_entries]
        from ccharts import Chart

try:
    import numpy as np
    import pandas as pd
    HAS_PANDAS = True
except ImportError:
    HAS_PANDAS = False

# Same five daily candles as tests/test_python_api.py, in columnar form.
OHLC = {
    "open": [328.75, 330.0, 317.25, 320.0, 306.0],
    "high": [330.0, 330.25, 321.0, 328.75, 307.25],
    "low": [323.75, 317.5, 314.5, 317.75, 300.75],
    "close": [328.0, 317.5, 321.0, 318.0, 301.0],
}
DAYS = ["2026-07-20", "2026-07-21", "2026-07-22", "2026-07-23", "2026-07-24"]

SAMPLE_JSON = """[
  {"ts": "2026-07-20T00:00:00+00:00", "open": 328.75, "high": 330.0, "low": 323.75, "close": 328.0},
  {"ts": "2026-07-21T00:00:00+00:00", "open": 330.0, "high": 330.25, "low": 317.5, "close": 317.5},
  {"ts": "2026-07-22T00:00:00+00:00", "open": 317.25, "high": 321.0, "low": 314.5, "close": 321.0},
  {"ts": "2026-07-23T00:00:00+00:00", "open": 320.0, "high": 328.75, "low": 317.75, "close": 318.0},
  {"ts": "2026-07-24T00:00:00+00:00", "open": 306.0, "high": 307.25, "low": 300.75, "close": 301.0}
]"""


def frame(tz=None, columns=None, index=True):
    """DataFrame of the sample candles, optionally tz-aware or renamed."""
    data = dict(OHLC)
    if columns is not None:
        data = {new: data[old] for old, new in zip(("open", "high", "low", "close"),
                                                   columns)}
    df = pd.DataFrame(data)
    if index:
        idx = pd.to_datetime(DAYS)
        idx = idx.tz_localize(tz if tz is not None else "UTC")
        df.index = idx
    return df


@unittest.skipUnless(HAS_PANDAS, "pandas is not installed")
class TestFromDataFrame(unittest.TestCase):
    def test_matches_json_path_exactly(self):
        # A DataFrame and the equivalent JSON must render identically.
        expected = Chart(SAMPLE_JSON)
        got = Chart.from_dataframe(frame())
        for kwargs in ({}, {"show_prices": True, "show_times": True},
                       {"width": 17, "height": 3}):
            for method in ("line", "candle"):
                self.assertEqual(getattr(expected, method)(**kwargs),
                                 getattr(got, method)(**kwargs))

    def test_case_insensitive_columns(self):
        df = frame(columns=("Open", "HIGH", "Low", "Close"))
        self.assertEqual(Chart.from_dataframe(df).line(),
                         Chart(SAMPLE_JSON).line())

    def test_explicit_column_names(self):
        df = frame(columns=("o", "h", "l", "c"))
        chart = Chart.from_dataframe(df, ohlc=("o", "h", "l", "c"))
        self.assertEqual(chart.line(), Chart(SAMPLE_JSON).line())

    def test_missing_column_raises(self):
        df = frame().drop(columns=["high"])
        with self.assertRaises(ValueError) as ctx:
            Chart.from_dataframe(df)
        self.assertIn("high", str(ctx.exception))

    def test_ambiguous_column_raises(self):
        df = frame()
        df["OPEN"] = df["open"]
        with self.assertRaises(ValueError):
            Chart.from_dataframe(df)

    def test_wrong_type_raises(self):
        with self.assertRaises(TypeError):
            Chart.from_dataframe({"open": [1]})

    def test_empty_frame_raises(self):
        with self.assertRaises(ValueError):
            Chart.from_dataframe(frame().iloc[:0])


@unittest.skipUnless(HAS_PANDAS, "pandas is not installed")
class TestDataFrameTimestamps(unittest.TestCase):
    def test_datetime_index_footer(self):
        out = Chart.from_dataframe(frame()).line(show_times=True)
        self.assertIn("2026-07-20", out)
        self.assertIn("2026-07-24", out)

    def test_tz_aware_index_shifts_to_utc(self):
        # Mirrors TestTimezones in test_python_api: midnight at +03:00 is the
        # previous day in UTC, and the footer shows the true instant.
        out = Chart.from_dataframe(frame(tz="Etc/GMT-3")).line(show_times=True)
        self.assertIn("2026-07-19", out)
        self.assertNotIn("2026-07-20", out)

    def test_plain_index_has_no_timestamps(self):
        out = Chart.from_dataframe(frame(index=False)).line(show_times=True)
        self.assertNotIn("2026", out)

    def test_epoch_column(self):
        df = frame(index=False)
        df["ts"] = [1784505600, 1784592000, 1784678400, 1784764800, 1784851200]
        out = Chart.from_dataframe(df, ts="ts").line(show_times=True)
        self.assertIn("2026-07-20", out)
        self.assertIn("2026-07-24", out)

    def test_string_datetime_column(self):
        df = frame(index=False)
        df["when"] = DAYS
        out = Chart.from_dataframe(df, ts="when").line(show_times=True)
        self.assertIn("2026-07-20", out)

    def test_missing_ts_column_raises(self):
        with self.assertRaises(ValueError):
            Chart.from_dataframe(frame(index=False), ts="nope")

    def test_nat_becomes_unknown(self):
        df = frame()
        idx = df.index.to_list()
        idx[2] = pd.NaT
        df.index = pd.DatetimeIndex(idx)
        # NaT maps to 0 ("unknown"), which must not crash or leak epoch 0.
        out = Chart.from_dataframe(df).line(show_times=True)
        self.assertIn("2026-07-20", out)
        self.assertNotIn("1970", out)


@unittest.skipUnless(HAS_PANDAS, "pandas is not installed")
class TestDataFrameNaN(unittest.TestCase):
    def _with_nan(self):
        df = frame()
        df.loc[df.index[0], "close"] = np.nan
        return df

    def test_dropna_drops_the_row(self):
        out = Chart.from_dataframe(self._with_nan()).line(show_times=True)
        # The first candle is gone, so the footer starts one day later.
        self.assertIn("2026-07-21", out)
        self.assertNotIn("2026-07-20", out)

    def test_dropna_false_raises(self):
        with self.assertRaises(ValueError):
            Chart.from_dataframe(self._with_nan(), dropna=False)

    def test_all_nan_raises(self):
        df = frame()
        df["close"] = np.nan
        with self.assertRaises(ValueError):
            Chart.from_dataframe(df)

    def test_inf_is_dropped_too(self):
        df = frame()
        df.loc[df.index[0], "high"] = np.inf
        out = Chart.from_dataframe(df).line(show_times=True)
        self.assertIn("2026-07-21", out)


if __name__ == "__main__":
    unittest.main()
