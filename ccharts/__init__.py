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

from ._core import (parse_json, parse_arrays, create_line, create_candle,
                    create_pie, create_hist, create_spark)


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

    @staticmethod
    def pie(labels, values, *, donut=False, colors=None, bg_color=None,
            show_legend=True, show_pct=False, width=24, height=10,
            slice_gap=0.0, inner_radius_ratio=None, legend_format=0,
            start_angle=None, counter_clockwise=False, center_text=None):
        """Draw a pie/donut chart and return it as a string.

        A pie is not OHLC data, so this is a static method: it takes the
        slices directly instead of reading ``self``.

        Args:
            labels: one label per slice (``str`` or ``None``), drawn in the
                legend when ``show_legend`` is set.
            values: one positive *amount* per slice; the pie computes the
                percentages. Any value ``<= 0`` (zero, negative, NaN) makes
                the whole render return the empty string.
            donut: True leaves the center hollow (a donut); False is a filled
                disk. Only consulted when ``inner_radius_ratio`` is None.
            colors: per-slice ANSI escape strings, or ``None`` for the fixed
                deterministic default palette.
            bg_color: background color of the cells outside the disk.
            show_legend: print one ``label  value (pct%)`` line per slice
                below the disk.
            show_pct: append ``(NN%)`` to each legend entry (format 0 only).
            width, height: chart size in cells (positive integers).
            slice_gap: angular gap between slices in radians; 0 = adjacent.
            inner_radius_ratio: donut thickness as a fraction of the outer
                radius, in ``[0, 1]``. ``0`` = filled disk; ``(0, 1]`` =
                hollow center of that thickness. ``None`` = donut's default
                (0.5) when ``donut`` is True, else a disk.
            legend_format: one of 0 (label  value  (+%(pct)))
                ``"label  value (pct%)"``, 1 ``"label  NN%"``,
                2 ``"value  (NN%)"``, 3 ``"label"`` only; 0 reproduces the
                original output. Values outside 0..3 fall back to 0.
            start_angle: angle in radians where slice 0 begins; ``None`` =
                ``CC_PI/2`` (12 o'clock).
            counter_clockwise: False = default counter-clockwise sweep; True =
                mirrored (clockwise) sweep.
            center_text: text drawn in the hollow center. Only shown when the
                disk has a real hollow (inner_radius_ratio > 0); ``None``/``""``
                disables it and it is truncated to fit the hole.

        Raises ``ValueError`` for non-finite values (NaN/inf in the slice
        values or the ``slice_gap``/``inner_radius_ratio``/``start_angle``
        options — these would corrupt the angle math), mismatched label/value
        lengths, and invalid dimensions.
        """
        _check_dimensions(width, height)
        return create_pie(
            labels, values, width, height,
            int(donut), colors, bg_color,
            int(show_legend), int(show_pct),
            float(slice_gap),
            -1.0 if inner_radius_ratio is None else float(inner_radius_ratio),
            int(legend_format),
            -1.0 if start_angle is None else float(start_angle),
            int(counter_clockwise),
            center_text,
        )

    @staticmethod
    def histogram(samples, *, width=60, height=8, bin_count=0,
                  min_value=None, max_value=None, rise_color=None,
                  bg_color=None, show_bins=False, show_prices=False,
                  plain=False):
        """Draw a histogram of a scalar sample sequence and return it as a
        string.

        A histogram is not OHLC data, so this is a static method: it takes
        the raw samples directly instead of reading ``self``.

        Args:
            samples: one scalar value per observation — a list, tuple,
                numpy array, array.array or any other sized sequence.
            width, height: chart size in cells (positive integers).
            bin_count: number of equal-width bins across the value window;
                ``0`` (or negative) auto-selects (20 for ``>= 40`` samples,
                else 10, trimmed to the width).
            min_value, max_value: the value window to bin across.
                ``None`` auto-selects that endpoint from the data range.
            rise_color: ANSI escape string for the bar fill (None = green).
            bg_color: ANSI escape string for the empty cells below a bar
                (None = terminal background).
            show_bins: append a value-axis footer row under the chart
                (window min on the left, window max on the right).
            show_prices: print the max-count / min-count labels in an
                8-column left margin (a vertical count axis).
            plain: return text with no ANSI escapes at all, overriding every
                color.

        Raises ``ValueError`` for non-finite samples (NaN/inf would corrupt
        the bin math), mismatched/non-numeric items, and invalid dimensions.
        An empty ``samples`` sequence renders the empty string.
        """
        _check_dimensions(width, height)
        if plain:
            rise_color = bg_color = ""
        return create_hist(
            samples, width, height,
            rise_color, bg_color,
            int(bin_count),
            float("nan") if min_value is None else float(min_value),
            float("nan") if max_value is None else float(max_value),
            int(show_bins), int(show_prices),
        )

    @staticmethod
    def sparkline(samples, *, width=60, height=1, rise_color=None,
                  area_color=None, min_above=0, min_below=0, plain=False):
        """Draw a tiny axis-less trend line from a series of values and return
        it as a string.

        A sparkline is not OHLC data, so this is a static method: it takes the
        raw samples directly instead of reading ``self``. Each column averages
        the samples in its span (exactly the line chart's downsampling), then
        maps the average to an 8-sub-pixel row, so a ``height=1`` sparkline is
        still smooth.

        Args:
            samples: one value per step — a list, tuple, numpy array,
                array.array or any other sized sequence.
            width, height: chart size in cells (positive integers). ``height``
                is expected small (default ``1``, up to ~3).
            rise_color: ANSI escape string for the line (None = green).
            area_color: ANSI escape string for a faint fill under the line,
                or None for no fill (default).
            min_above, min_below: reserved sub-pixels at the top/bottom edge
                so the line does not clip against the edge at tiny heights
                (both default to ``0``).
            plain: return text with no ANSI escapes at all, overriding every
                color.

        Raises ``ValueError`` for non-finite samples (NaN/inf would corrupt
        the column math), mismatched/non-numeric items, and invalid
        dimensions. An empty ``samples`` sequence renders the empty string.
        """
        _check_dimensions(width, height)
        if plain:
            rise_color = area_color = ""
        return create_spark(
            samples, width, height,
            rise_color, area_color,
            int(min_above), int(min_below),
        )