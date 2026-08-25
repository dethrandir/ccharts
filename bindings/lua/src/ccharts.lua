-- ccharts.lua — high-level Lua facade for the ccharts charting library.
--
-- This is a thin, pure-Lua layer over the compiled C module (ccharts_core,
-- built against the vendored C ABI). It owns the fiddly color-name resolution
-- (color names -> ANSI escapes) and the friendly, table-based API; the C
-- module owns all struct/array marshalling (compiling the vendored ABI and
-- building contiguous struct blocks for pie/bar/stack/box).

local core = require "ccharts_core"

local ccharts = {}

-- Color name -> ccharts_color() index (mirrors ccharts_color_index in the ABI).
local COLOR = {
  black = 0, red = 1, green = 2, yellow = 3, blue = 4, magenta = 5,
  cyan = 6, white = 7, bright_black = 8, bright_red = 9, bright_green = 10,
  bright_yellow = 11, bright_blue = 12, bright_magenta = 13, bright_cyan = 14,
  bright_white = 15, reset = 16,
}

-- Resolve a colour option to the ANSI escape string the C layer expects.
-- nil -> nil (library default); a colour name -> its escape; anything else
-- (already an escape string) is passed through.
local function resolve(v)
  if v == nil then return nil end
  if type(v) == "string" then
    local idx = COLOR[v]
    if idx ~= nil then return core._color(idx) end
    return v -- an arbitrary ANSI/truecolor escape
  end
  return tostring(v)
end

local EMPTY = "" -- plain mode: emit no escape at all (distinct from NULL)

-- ------------------------------------------------------------------
-- Dataset wrapper (line / candle / size)
-- ------------------------------------------------------------------
local data_mt = {}
data_mt.__index = data_mt

function data_mt.size(self) return core._data_len(self._h) end
data_mt.len = data_mt.size

local function chart_settings(s)
  s = s or {}
  if s.plain then
    return {
      rise_color = EMPTY, fall_color = EMPTY, bg_color = EMPTY,
      area_color = EMPTY, single_color = s.single_color or false,
      show_prices = s.show_prices or false, show_times = s.show_times or false,
    }
  end
  return {
    rise_color = resolve(s.rise_color), fall_color = resolve(s.fall_color),
    bg_color = resolve(s.bg_color), area_color = resolve(s.area_color),
    single_color = s.single_color or false, show_prices = s.show_prices or false,
    show_times = s.show_times or false,
  }
end

function data_mt.line(self, width, height, settings)
  return core._line(self._h, width, height, chart_settings(settings))
end
function data_mt.candle(self, width, height, settings)
  return core._candle(self._h, width, height, chart_settings(settings))
end

-- ------------------------------------------------------------------
-- Building a dataset
-- ------------------------------------------------------------------

-- Build a dataset from four equal-length price columns and optional epoch
-- seconds. `ts` may be nil (all timestamps unknown).
function ccharts.from_arrays(open, high, low, close, ts)
  if not open or #open == 0 then error("need at least one candle") end
  return setmetatable({ _h = core._from_arrays(open, high, low, close, ts) },
                      data_mt)
end

-- Build a dataset from the fixed-schema JSON accepted by the C layer.
function ccharts.from_json(text)
  return setmetatable({ _h = core._from_json(text) }, data_mt)
end

-- Build a dataset from CSV rows of `open,high,low,close[,timestamp]`.
function ccharts.from_csv(text, value_separator, line_separator)
  local vs = value_separator and string.byte(value_separator, 1) or 44  -- ","
  local ls = line_separator and string.byte(line_separator, 1) or 10    -- "\n"
  return setmetatable({ _h = core._from_csv(text, vs, ls) }, data_mt)
end

-- ------------------------------------------------------------------
-- Standalone renderers (no OHLC dataset)
-- ------------------------------------------------------------------

-- pie: slices = {{label=, value=}, ...}
function ccharts.pie(slices, width, height, settings)
  if not slices or #slices == 0 then error("need at least one slice") end
  local s = settings or {}
  local out = {
    donut = s.donut or false,
    show_legend = (s.show_legend == nil) and true or s.show_legend,
    show_pct = s.show_pct or false,
  }
  if s.slice_gap ~= nil then out.slice_gap = s.slice_gap end
  if s.inner_radius_ratio ~= nil then out.inner_radius_ratio = s.inner_radius_ratio end
  if s.legend_format ~= nil then out.legend_format = s.legend_format end
  if s.start_angle ~= nil then out.start_angle = s.start_angle end
  if s.counter_clockwise then out.counter_clockwise = true end
  if s.center_text then out.center_text = s.center_text end
  if s.colors then
    local resolved = {}
    for i, c in ipairs(s.colors) do resolved[i] = resolve(c) end
    out.colors = resolved
  end
  return core._pie(slices, width, height, out)
end

-- histogram: samples = list of numbers
function ccharts.histogram(samples, width, height, settings)
  if not samples or #samples == 0 then error("need at least one sample") end
  local s = settings or {}
  local rc, bg
  if s.plain then rc, bg = EMPTY, EMPTY else rc, bg = resolve(s.rise_color), resolve(s.bg_color) end
  return core._hist(samples, width, height, {
    rise_color = rc, bg_color = bg, bin_count = s.bin_count or 0,
    min_value = (s.min_value ~= nil and s.min_value) or (0 / 0), -- NaN = auto
    max_value = (s.max_value ~= nil and s.max_value) or (0 / 0),
    show_bins = s.show_bins or false, show_prices = s.show_prices or false,
  })
end

-- sparkline: samples = list of numbers
function ccharts.sparkline(samples, width, height, settings)
  if not samples or #samples == 0 then error("need at least one sample") end
  local s = settings or {}
  local rc, ac
  if s.plain then rc, ac = EMPTY, EMPTY else rc, ac = resolve(s.rise_color), resolve(s.area_color) end
  return core._spark(samples, width, height, {
    rise_color = rc, area_color = ac,
    min_above = s.min_above or 0, min_below = s.min_below or 0,
  })
end

-- bar: items = {{label=, value=}, ...}
function ccharts.bar(items, width, height, settings)
  if not items or #items == 0 then error("need at least one bar") end
  local s = settings or {}
  local rc, bg
  if s.plain then rc, bg = EMPTY, EMPTY else rc, bg = resolve(s.rise_color), resolve(s.bg_color) end
  return core._bar(items, width, height, {
    rise_color = rc, bg_color = bg,
    show_labels = s.show_labels or false, show_prices = s.show_prices or false,
  })
end

-- stacked_bar: series = {{name=, values={...}}, ...}
function ccharts.stacked_bar(series, width, height, settings)
  if not series or #series == 0 then error("need at least one series") end
  local s = settings or {}
  local colors
  if s.plain then
    colors = {}
    for i = 1, #series do colors[i] = EMPTY end
  elseif s.colors then
    colors = {}
    for i, c in ipairs(s.colors) do colors[i] = resolve(c) end
  end
  return core._stack(series, width, height, {
    colors = colors, bg_color = (s.plain and EMPTY) or resolve(s.bg_color),
    cat_labels = s.cat_labels, show_labels = s.show_labels or false,
    show_prices = s.show_prices or false,
  })
end

-- heatmap: values = rows x cols matrix (array of equal-length arrays)
function ccharts.heatmap(values, width, height, settings)
  if not values or #values == 0 then error("need a non-empty value matrix") end
  local rows, cols = #values, #values[1]
  if cols == 0 then error("matrix columns must not be empty") end
  for r = 1, rows do
    if #values[r] ~= cols then error("all rows must have the same number of values") end
  end
  local s = settings or {}
  local function pick(k)
    if s.plain then return EMPTY end
    return resolve(s[k])
  end
  return core._heat(values, rows, cols, width, height, {
    low_color = pick("low_color"), high_color = pick("high_color"),
    mid_color = pick("mid_color"), bg_color = pick("bg_color"),
    row_labels = s.row_labels, col_labels = s.col_labels,
    show_labels = s.show_labels or false,
  })
end

-- boxplot: categories = {{name=, samples={...}}, ...}
function ccharts.boxplot(categories, width, height, settings)
  if not categories or #categories == 0 then error("need at least one category") end
  local s = settings or {}
  local rc, ac, bg
  if s.plain then rc, ac, bg = EMPTY, EMPTY, EMPTY
  else rc, ac, bg = resolve(s.rise_color), resolve(s.area_color), resolve(s.bg_color) end
  return core._box(categories, width, height, {
    rise_color = rc, area_color = ac, bg_color = bg,
    show_prices = s.show_prices or false,
  })
end

-- ------------------------------------------------------------------
-- Introspection
-- ------------------------------------------------------------------
ccharts.version = core._version()
ccharts.max_dim = core._max_dim()
ccharts.max_cells = core._max_cells()
ccharts._core = core

return ccharts