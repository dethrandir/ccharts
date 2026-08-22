"""ccharts — financial OHLC data as a string (Python bindings).

Thin wrapper around the C single-header library `ccharts.h`. Parse OHLC data
once, then render line or candlestick charts as text: what comes back is a
plain ``str``, so it can be printed, logged, embedded in an HTML block or
written to a file. Nothing is printed for you.

Quickstart:
    from ccharts import Chart

    chart = Chart(open("prices.txt").read())   # fixed-schema JSON
    print(chart.line(width=60, height=8, show_prices=True, show_times=True))
    print(chart.candle(width=60, height=8))

Columnar data skips JSON entirely and is copied straight into the C array:

    Chart.from_arrays(opens, highs, lows, closes, ts=epoch_seconds)
    Chart.from_dataframe(df)                   # requires pandas

``from_arrays`` accepts lists, numpy arrays, array.array — anything sized and
either float64-buffer-backed or iterable. pandas is an optional dependency
(``pip install ccharts[pandas]``) and is imported only when
``from_dataframe`` is called.

All color and flag arguments are optional. Colors are ANSI escape strings
(e.g. ``"\\x1b[34m"``); the ``CC_COLOR_*`` constants from the C header have
no Python equivalent, so pass raw escapes or ``None`` for the defaults
(green/red). Pass ``plain=True`` for text with no escapes at all, for output
that is not going to a terminal.

ISO8601 timestamps may carry a UTC offset (``+03:00``, ``Z``, ...); the
offset is applied, so `show_times` always displays the true UTC instant.

Chart dimensions must be positive integers; the C library additionally caps
width/height at 100000 cells per side and 1000000 total cells
(``CC_MAX_DIM`` / ``CC_MAX_CELLS`` in ccharts.h).
"""

from ._core import parse_json, parse_arrays, create_line, create_candle


def _check_dimensions(width, height):
    """Cheap Python-side validation before handing the call to the C layer.

    Raises TypeError for non-integers and ValueError for non-positive
    dimensions. Also rejects booleans (which are ints in Python).
    """
    if (not isinstance(width, int) or isinstance(width, bool) or
            not isinstance(height, int) or isinstance(height, bool)):
        raise TypeError("width and height must be integers")
    if width <= 0 or height <= 0:
        raise ValueError("width and height must be positive integers")


class Chart:
    """A parsed OHLC dataset that can be rendered as a line or candle chart."""

    @classmethod
    def _from_capsule(cls, capsule):
        """Wraps an already-built C capsule, bypassing the JSON constructor."""
        self = cls.__new__(cls)
        self._capsule = capsule
        return self

    @classmethod
    def from_arrays(cls, open, high, low, close, ts=None):
        """Build a Chart from four equal-length price columns.

        Each column may be a list, tuple, numpy array, array.array or any
        other sized sequence; float64 C-contiguous buffers are copied in one
        block, everything else element by element. ``ts`` is optional epoch
        seconds (0 means "unknown", which suppresses the ``show_times``
        footer).

        Raises ValueError on empty or mismatched columns and on non-finite
        values (NaN/inf would corrupt the renderer's pixel math).
        """
        if ts is None:
            return cls._from_capsule(parse_arrays(open, high, low, close))
        return cls._from_capsule(parse_arrays(open, high, low, close, ts))

    @classmethod
    def from_dataframe(cls, df, ohlc=None, ts=None, dropna=True):
        """Build a Chart from a pandas DataFrame.

        Args:
            df: the DataFrame. Row order is used as-is — nothing is sorted,
                so a descending frame renders right-to-left.
            ohlc: explicit ``(open, high, low, close)`` column names. When
                None the columns are matched case-insensitively by name.
            ts: name of a timestamp column. When None, a DatetimeIndex is
                used if present; otherwise the chart has no timestamps.
            dropna: True drops rows with NaN in any OHLC column (indicator
                frames usually have NaN warm-up rows); False raises
                ValueError instead.

        Requires pandas (``pip install ccharts[pandas]``); it is imported
        here, not at module import time.
        """
        from ._pandas import dataframe_to_arrays

        o, h, l, c, epoch = dataframe_to_arrays(df, ohlc=ohlc, ts=ts,
                                                dropna=dropna)
        return cls.from_arrays(o, h, l, c, ts=epoch)

    def __init__(self, json_data: str):
        """Parse the given JSON string in C and hold the result in memory.

        The JSON must match the fixed schema expected by ``cc_json_to_ohlc``:
        an array of objects with ``ts``, ``open``, ``high``, ``low`` and
        ``close`` fields (``volume`` is ignored). Raises ``ValueError`` if
        the data cannot be parsed or is empty.
        """
        if not isinstance(json_data, str):
            raise TypeError("json_data must be a string")
        self._capsule = parse_json(json_data)

    def line(self, width=60, height=8, rise_color=None, fall_color=None,
             bg_color=None, area_color=None, single_color=False,
             show_prices=False, show_times=False, plain=False):
        """Draw a line chart and return it as a string.

        Args:
            width, height: chart plot area in cells (positive integers).
            rise_color, fall_color: ANSI escape strings for rising/falling
                segments (None = green/red).
            bg_color: background color of empty cells (None = terminal bg).
            area_color: fill the area below the line with this color.
            single_color: True = one color for the whole chart, chosen from
                the overall change; False = color each segment individually.
            show_prices: print max/min price labels in a left margin.
            show_times: print first/last timestamp under the chart.
            plain: return text with no ANSI escapes at all, overriding every
                color. Use it when the chart is going somewhere that does not
                interpret escapes — a log file, an HTML block, a commit
                message.
        """
        _check_dimensions(width, height)
        if plain:
            rise_color = fall_color = bg_color = area_color = ""
        return create_line(
            self._capsule, width, height,
            rise_color, fall_color, bg_color, area_color,
            int(single_color), int(show_prices), int(show_times)
        )

    def candle(self, width=60, height=8, rise_color=None, fall_color=None,
               bg_color=None, area_color=None, single_color=False,
               show_prices=False, show_times=False, plain=False):
        """Draw a candle chart and return it as a string.

        Same arguments as :meth:`line`. When ``width >= len(data)`` each
        candle is several cells wide with a gap between neighbors; otherwise
        neighboring candles are aggregated to fit.
        """
        _check_dimensions(width, height)
        if plain:
            rise_color = fall_color = bg_color = area_color = ""
        return create_candle(
            self._capsule, width, height,
            rise_color, fall_color, bg_color, area_color,
            int(single_color), int(show_prices), int(show_times)
        )