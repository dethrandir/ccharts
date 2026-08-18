from ._core import parse_json, create_line, create_candle

class Chart:
    def __init__(self, json_data: str):
        """Parse the given JSON string in C and hold the result in memory."""
        self._capsule = parse_json(json_data)

    def line(self, width=60, height=8, rise_color=None, fall_color=None,
             bg_color=None, area_color=None, single_color=False,
             show_prices=False, show_times=False):
        """Draw a line chart and return it as a string."""
        return create_line(
            self._capsule, width, height,
            rise_color, fall_color, bg_color, area_color,
            int(single_color), int(show_prices), int(show_times)
        )

    def candle(self, width=60, height=8, rise_color=None, fall_color=None,
               bg_color=None, area_color=None, single_color=False,
               show_prices=False, show_times=False):
        """Draw a candle chart and return it as a string."""
        return create_candle(
            self._capsule, width, height,
            rise_color, fall_color, bg_color, area_color,
            int(single_color), int(show_prices), int(show_times)
        )
