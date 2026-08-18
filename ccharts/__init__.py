"""ccharts — terminal charts for financial OHLC data (Python bindings).

Thin wrapper around the C single-header library `ccharts.h`. Parse OHLC data
once, then render line or candlestick charts as ANSI-colored strings that can
be printed directly to a terminal.

Quickstart:
    from ccharts import Chart

    chart = Chart(open("prices.txt").read())   # fixed-schema JSON
    print(chart.line(width=60, height=8, show_prices=True, show_times=True))
    print(chart.candle(width=60, height=8))

All color and flag arguments are optional. Colors are ANSI escape strings
(e.g. ``"\\x1b[34m"``); the ``CC_COLOR_*`` constants from the C header have
no Python equivalent, so pass raw escapes or ``None`` for the defaults
(green/red).
"""

from ._core import parse_json, create_line, create_candle


class Chart:
    """A parsed OHLC dataset that can be rendered as a line or candle chart."""

    def __init__(self, json_data: str):
        """Parse the given JSON string in C and hold the result in memory.

        The JSON must match the fixed schema expected by ``cc_json_to_ohlc``:
        an array of objects with ``ts``, ``open``, ``high``, ``low`` and
        ``close`` fields (``volume`` is ignored). Raises ``ValueError`` if
        the data cannot be parsed.
        """
        self._capsule = parse_json(json_data)

    def line(self, width=60, height=8, rise_color=None, fall_color=None,
             bg_color=None, area_color=None, single_color=False,
             show_prices=False, show_times=False):
        """Draw a line chart and return it as a string.

        Args:
            width, height: chart plot area in cells.
            rise_color, fall_color: ANSI escape strings for rising/falling
                segments (None = green/red).
            bg_color: background color of empty cells (None = terminal bg).
            area_color: fill the area below the line with this color.
            single_color: True = one color for the whole chart, chosen from
                the overall change; False = color each segment individually.
            show_prices: print max/min price labels in a left margin.
            show_times: print first/last timestamp under the chart.
        """
        return create_line(
            self._capsule, width, height,
            rise_color, fall_color, bg_color, area_color,
            int(single_color), int(show_prices), int(show_times)
        )

    def candle(self, width=60, height=8, rise_color=None, fall_color=None,
               bg_color=None, area_color=None, single_color=False,
               show_prices=False, show_times=False):
        """Draw a candle chart and return it as a string.

        Same arguments as :meth:`line`. When ``width >= len(data)`` each
        candle is several cells wide with a gap between neighbors; otherwise
        neighboring candles are aggregated to fit.
        """
        return create_candle(
            self._capsule, width, height,
            rise_color, fall_color, bg_color, area_color,
            int(single_color), int(show_prices), int(show_times)
        )
