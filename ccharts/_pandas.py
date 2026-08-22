"""DataFrame -> columnar OHLC conversion for `Chart.from_dataframe`.

This module is imported lazily by ccharts/__init__.py, never at package
import time: pandas is an optional dependency (`pip install ccharts[pandas]`)
and the package must stay usable — and installable — without it.

The output is what `ccharts._core.parse_arrays` wants: four C-contiguous
float64 arrays plus an optional int64 array of epoch seconds. Nothing is
serialized to JSON on the way.
"""

import numpy as np
import pandas as pd

_OHLC_NAMES = ("open", "high", "low", "close")


def _resolve_columns(df, ohlc):
    """Returns the four column labels to read, or raises ValueError."""
    if ohlc is not None:
        cols = tuple(ohlc)
        if len(cols) != 4:
            raise ValueError(
                "ohlc must name exactly four columns (open, high, low, close)")
        missing = [c for c in cols if c not in df.columns]
        if missing:
            raise ValueError("columns not in DataFrame: %r (available: %r)"
                             % (missing, list(df.columns)))
        return cols

    # Case-insensitive match on string labels. Non-string labels (e.g. the
    # tuples of a MultiIndex) simply never match, which lands the caller in
    # the "pass ohlc=(...)" error below.
    by_lower = {}
    for col in df.columns:
        if isinstance(col, str):
            by_lower.setdefault(col.lower(), []).append(col)

    resolved = []
    for name in _OHLC_NAMES:
        matches = by_lower.get(name, [])
        if not matches:
            raise ValueError(
                "no %r column in the DataFrame (available: %r); "
                "pass ohlc=(open, high, low, close) to name them explicitly"
                % (name, list(df.columns)))
        if len(matches) > 1:
            raise ValueError(
                "ambiguous %r column: %r; pass ohlc=(...) to disambiguate"
                % (name, matches))
        resolved.append(matches[0])
    return tuple(resolved)


def _epoch_seconds(values):
    """datetime-like -> int64 epoch seconds. NaT becomes 0 ("unknown").

    Timezone-aware input is converted to UTC first, so the stored instant
    matches what cc_iso8601_to_epoch() would produce for the same moment
    written as an ISO8601 string with an offset. Naive input is treated as
    UTC, the same rule the C parser uses for a missing offset.
    """
    idx = pd.DatetimeIndex(values)
    if idx.tz is not None:
        idx = idx.tz_convert("UTC").tz_localize(None)

    # Going through numpy rather than idx.astype("int64") on purpose: in
    # pandas 2 a DatetimeIndex can carry a non-nanosecond unit and astype
    # returns the raw value *in that unit*, so the same code would produce
    # seconds, millis or nanos depending on the frame. numpy's datetime64
    # cast normalizes the unit for every pandas version.
    epoch = np.asarray(idx).astype("datetime64[s]").astype("int64")
    epoch[np.asarray(idx.isna())] = 0
    return epoch


def _timestamps(df, ts):
    """Epoch-second array for the frame, or None when there are none."""
    if ts is not None:
        if ts not in df.columns:
            raise ValueError("no %r column in the DataFrame (available: %r)"
                             % (ts, list(df.columns)))
        col = df[ts]
        if isinstance(col, pd.DataFrame):
            raise ValueError("ts column %r is duplicated in the DataFrame" % (ts,))
        if pd.api.types.is_numeric_dtype(col):
            # Already epoch seconds; NaN becomes 0 like NaT does.
            raw = col.to_numpy(dtype="float64")
            return np.where(np.isnan(raw), 0.0, raw).astype("int64")
        return _epoch_seconds(pd.to_datetime(col))

    if isinstance(df.index, pd.DatetimeIndex):
        return _epoch_seconds(df.index)
    return None


def dataframe_to_arrays(df, ohlc=None, ts=None, dropna=True):
    """Splits a DataFrame into (open, high, low, close, ts_or_None) arrays.

    Row order is preserved exactly; nothing is sorted. See
    Chart.from_dataframe for the argument semantics.
    """
    if not isinstance(df, pd.DataFrame):
        raise TypeError("df must be a pandas DataFrame")
    if len(df) == 0:
        raise ValueError("DataFrame has no rows")

    cols = _resolve_columns(df, ohlc)

    arrays = []
    for name in cols:
        col = df[name]
        if isinstance(col, pd.DataFrame):
            raise ValueError("column %r is duplicated in the DataFrame" % (name,))
        arrays.append(col.to_numpy(dtype="float64"))

    epoch = _timestamps(df, ts)

    # NaN/inf must not reach the renderer (parse_arrays rejects them too).
    bad = np.zeros(len(df), dtype=bool)
    for a in arrays:
        bad |= ~np.isfinite(a)
    if bad.any():
        if not dropna:
            raise ValueError(
                "OHLC columns contain NaN or inf; pass dropna=True to drop "
                "those rows")
        keep = ~bad
        arrays = [a[keep] for a in arrays]
        if epoch is not None:
            epoch = epoch[keep]
        if len(arrays[0]) == 0:
            raise ValueError("no rows left after dropping NaN/inf OHLC values")

    # parse_arrays' fast path needs one contiguous float64 block per column;
    # a Series sliced out of a consolidated block can be strided.
    arrays = [np.ascontiguousarray(a, dtype="float64") for a in arrays]
    if epoch is not None:
        epoch = np.ascontiguousarray(epoch, dtype="int64")

    return arrays[0], arrays[1], arrays[2], arrays[3], epoch
