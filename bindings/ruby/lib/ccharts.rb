# frozen_string_literal: true

# ccharts — financial OHLC data as a string.
#
# Draws line, candlestick, pie/donut, histogram, sparkline, bar, stacked-bar,
# heatmap and box-plot charts with Unicode block characters and optional ANSI
# color. Ships the vendored C ABI (ext/ccharts/vendor) which is compiled from
# source by mkmf and driven through Fiddle, so the runtime is dependency-free.
#
#   chart = Ccharts::Chart.from_arrays(open: [...], high: [...], low: [...], close: [...])
#   puts chart.line(60, 8)
#   puts Ccharts::Chart.pie([{ label: "A", value: 40 }, { label: "B", value: 60 }], 24, 10)
require_relative "ccharts/version"
require_relative "ccharts/color"
require_relative "ccharts/ffi"
require_relative "ccharts/settings"
require_relative "ccharts/chart"

require "fiddle"

module Ccharts
  # Raised when the C layer reports a non-OK status, carrying the library's
  # human-readable message.
  class Error < StandardError
    def self.for_status(status)
      msg = FFI.read_cstr(FFI::ERROR_MSG.call(status)) || "unknown ccharts error"
      new("#{msg} (status #{status})")
    end
  end

  # Raise if `status` is not CCHARTS_OK.
  def self.raise_on(status)
    return if status == 0
    raise Error.for_status(status)
  end

  # Version of the underlying C library.
  def self.version
    FFI.read_cstr(FFI::VERSION.call)
  end

  # CC_MAX_DIM / CC_MAX_CELLS limits exposed by the ABI.
  def self.max_dim
    FFI::MAX_DIM.call
  end

  def self.max_cells
    FFI::MAX_CELLS.call
  end
end
