-- test_conformance.lua — the shared 70-case conformance suite.
--
-- Renders every case in conformance/cases.json through this binding and
-- compares byte-for-byte against conformance/golden/<name>.txt — the contract
-- every ccharts binding is held to. A settings/struct or marshalling mismatch
-- shows up here as a byte difference, so this is the acceptance gate.

require "tests.helper"
local json = _G.json
local test = _G.test
local TinyTest = _G.TinyTest

local ccharts = require "ccharts"

local function floats(t)
  local out = {}
  for i, v in ipairs(t) do out[i] = v + 0 end
  return out
end

local function render_case(doc, case_data)
  local cfg = case_data.settings or {}
  local chart = case_data.chart

  if chart == "hist" then
    local s = {
      bin_count = cfg.bin_count or 0, min_value = cfg.min_value,
      max_value = cfg.max_value, show_bins = cfg.show_bins or false,
      show_prices = cfg.show_prices or false, plain = cfg.plain or false,
      rise_color = cfg.rise_color, bg_color = cfg.bg_color,
    }
    return ccharts.histogram(floats(case_data.samples),
                             case_data.width, case_data.height, s)

  elseif chart == "spark" then
    local s = {
      min_above = cfg.min_above or 0, min_below = cfg.min_below or 0,
      plain = cfg.plain or false, rise_color = cfg.rise_color,
      area_color = cfg.area_color,
    }
    return ccharts.sparkline(floats(case_data.samples),
                             case_data.width, case_data.height, s)

  elseif chart == "bar" then
    local items = {}
    for i, it in ipairs(case_data.items) do
      items[i] = { label = tostring(it.label), value = it.value + 0 }
    end
    local s = {
      show_labels = cfg.show_labels or false,
      show_prices = cfg.show_prices or false, plain = cfg.plain or false,
      rise_color = cfg.rise_color, bg_color = cfg.bg_color,
    }
    return ccharts.bar(items, case_data.width, case_data.height, s)

  elseif chart == "pie" then
    local slices = {}
    for i, sl in ipairs(case_data.slices) do
      slices[i] = { label = tostring(sl.label), value = sl.value + 0 }
    end
    local s = {
      donut = cfg.donut or false,
      show_legend = (cfg.show_legend == nil) and true or cfg.show_legend,
      show_pct = cfg.show_pct or false,
    }
    if cfg.slice_gap ~= nil then s.slice_gap = cfg.slice_gap end
    if cfg.inner_radius_ratio ~= nil then s.inner_radius_ratio = cfg.inner_radius_ratio end
    if cfg.legend_format ~= nil then s.legend_format = cfg.legend_format end
    if cfg.start_angle ~= nil then s.start_angle = cfg.start_angle end
    if cfg.counter_clockwise then s.counter_clockwise = cfg.counter_clockwise end
    if cfg.center_text then s.center_text = cfg.center_text end
    if cfg.colors then s.colors = cfg.colors end
    return ccharts.pie(slices, case_data.width, case_data.height, s)

  elseif chart == "stack" then
    local series = {}
    for i, sr in ipairs(case_data.series) do
      series[i] = { name = tostring(sr.name), values = floats(sr.values) }
    end
    local s = {
      show_labels = cfg.show_labels or false,
      show_prices = cfg.show_prices or false, plain = cfg.plain or false,
      bg_color = cfg.bg_color,
    }
    if cfg.colors then s.colors = cfg.colors end
    if cfg.cat_labels then s.cat_labels = cfg.cat_labels end
    return ccharts.stacked_bar(series, case_data.width, case_data.height, s)

  elseif chart == "heat" then
    local matrix = {}
    for i, row in ipairs(case_data.values) do matrix[i] = floats(row) end
    local s = {
      show_labels = cfg.show_labels or false, plain = cfg.plain or false,
      low_color = cfg.low_color, high_color = cfg.high_color,
      mid_color = cfg.mid_color, bg_color = cfg.bg_color,
    }
    if cfg.row_labels then s.row_labels = cfg.row_labels end
    if cfg.col_labels then s.col_labels = cfg.col_labels end
    return ccharts.heatmap(matrix, case_data.width, case_data.height, s)

  elseif chart == "box" then
    local cats = {}
    for i, c in ipairs(case_data.categories) do
      cats[i] = { name = tostring(c.name), samples = floats(c.samples) }
    end
    local s = {
      show_prices = cfg.show_prices or false, plain = cfg.plain or false,
      rise_color = cfg.rise_color, area_color = cfg.area_color,
      bg_color = cfg.bg_color,
    }
    return ccharts.boxplot(cats, case_data.width, case_data.height, s)

  else -- line / candle
    local dataset = doc.datasets[case_data.dataset]
    local data
    if case_data.source == "json" then
      data = ccharts.from_json(dataset.json)
    else
      data = ccharts.from_arrays(
        floats(dataset.open), floats(dataset.high),
        floats(dataset.low), floats(dataset.close), dataset.ts)
    end
    local s = {
      single_color = cfg.single_color or false,
      show_prices = cfg.show_prices or false,
      show_times = cfg.show_times or false, plain = cfg.plain or false,
      rise_color = cfg.rise_color, fall_color = cfg.fall_color,
      bg_color = cfg.bg_color, area_color = cfg.area_color,
    }
    if chart == "line" then
      return data:line(case_data.width, case_data.height, s)
    else
      return data:candle(case_data.width, case_data.height, s)
    end
  end
end

test("all conformance cases match the goldens byte-for-byte", function()
  local dir = conformance_dir()
  assert(io.open(dir .. "/cases.json"), "conformance suite not present at " .. dir)
  local f = io.open(dir .. "/cases.json", "r")
  local text = f:read("*a")
  f:close()
  local doc = json.decode(text)
  local cases = doc.cases
  assert(#cases >= 70, "conformance suite looks truncated (" .. #cases .. " cases)")

  local failures = {}
  for _, case_data in ipairs(cases) do
    local name = case_data.name
    local ok, rendered = pcall(render_case, doc, case_data)
    if not ok then
      failures[#failures + 1] = name .. " (render error: " .. rendered .. ")"
    else
      local g = io.open(dir .. "/golden/" .. name .. ".txt", "rb")
      assert(g, "missing golden file for " .. name)
      local expected = g:read("*a")
      g:close()
      if rendered ~= expected then
        failures[#failures + 1] = name
      end
    end
  end

  if #failures > 0 then
    error(#failures .. " case(s) differ from the goldens: "
          .. table.concat(failures, ", "))
  end
end)

TinyTest.run()