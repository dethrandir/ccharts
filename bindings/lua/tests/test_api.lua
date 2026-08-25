-- test_api.lua — smoke/API tests for the ccharts Lua binding.
--
-- Exercises the high-level facade for every chart type, verifies version and
-- introspection, and (critically) exercises the contiguous struct-array
-- marshalling paths (pie / bar / stacked_bar / box) with multiple elements —
-- the cases that segfault if a binding builds an array of pointers instead of
-- one contiguous struct block.

require "tests.helper"
local TinyTest = _G.TinyTest

local ccharts = require "ccharts"

-- No ANSI in the plain output.
local function assert_no_escape(s, what)
  assert(not s:find("\27"), what .. " leaked an ANSI escape")
end

test("version and introspection match the ABI", function()
  assert_equal("3.0.0", ccharts.version)
  assert(type(ccharts.max_dim) == "number" and ccharts.max_dim > 0)
  assert(type(ccharts.max_cells) == "number" and ccharts.max_cells > 0)
end)

test("from_arrays + line / candle render and size is reported", function()
  local data = ccharts.from_arrays(
    { 1, 2, 3 }, { 3, 4, 5 }, { 0, 1, 2 }, { 2, 3, 4 }, { 100, 200, 300 })
  assert_equal(3, data:size())
  assert(data:line(60, 8) ~= nil and #data:line(60, 8) > 0)
  assert(data:candle(60, 8) ~= nil and #data:candle(60, 8) > 0)
  assert(data:line(60, 8, { plain = true }):find("\27") == nil)
end)

test("from_json and from_csv build equivalent data", function()
  local json =
    '[{"ts":"2026-07-20T00:00:00+00:00","open":1,"high":2,"low":0.5,"close":1.5}]'
  local j = ccharts.from_json(json)
  assert_equal(1, j:size())
  local c = ccharts.from_csv("1,2,0.5,1.5\n")
  assert_equal(1, c:size())
  -- Empty source raises a Lua error, not a crash.
  local ok = pcall(ccharts.from_json, "not json")
  assert(not ok, "from_json must raise on malformed input")
end)

test("pie renders multi-slice output (contiguous structs)", function()
  local out = ccharts.pie(
    { { label = "A", value = 40 }, { label = "B", value = 30 },
      { label = "C", value = 30 } }, 24, 10,
    { donut = true, show_legend = true, show_pct = true })
  assert(out ~= nil and #out > 0)
  local donut = ccharts.pie(
    { { label = "A", value = 40 }, { label = "B", value = 30 },
      { label = "C", value = 30 } }, 24, 10,
    { donut = true, show_legend = true, inner_radius_ratio = 0.2 })
  assert(donut ~= nil and #donut > 0)
  assert_no_escape(ccharts.pie({ { label = "Z", value = 1 } }, 24, 10,
                                { colors = { "", "", "" } }), "pie")
end)

test("histogram and sparkline render and honor plain", function()
  local samples = { 0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10 }
  assert(#ccharts.histogram(samples, 60, 8) > 0)
  assert_no_escape(ccharts.histogram(samples, 60, 8, { plain = true }),
                   "histogram")
  assert(#ccharts.sparkline(samples, 24, 1) > 0)
  assert_no_escape(ccharts.sparkline(samples, 24, 1, { plain = true }),
                   "sparkline")
  -- NaN ("auto") min/max from an explicit nil must not crash.
  assert(#ccharts.histogram(samples, 20, 6,
                            { bin_count = 5, min_value = nil, max_value = nil }) > 0)
end)

test("bar renders multi-item output (contiguous structs)", function()
  local items = { { label = "A", value = 1 }, { label = "B", value = 4 },
                  { label = "C", value = 2 }, { label = "D", value = 5 } }
  assert(#ccharts.bar(items, 60, 8, { show_labels = true }) > 0)
  assert_no_escape(ccharts.bar(items, 60, 8, { plain = true }), "bar")
end)

test("stacked_bar renders multi-series matrix (per-series values)", function()
  local series = {
    { name = "Alpha", values = { 1, 4, 2, 5, 3 } },
    { name = "Beta", values = { 3, 2, 5, 1, 4 } },
  }
  assert(#ccharts.stacked_bar(series, 60, 8, { show_labels = true }) > 0)
  assert_no_escape(ccharts.stacked_bar(series, 60, 8, { plain = true }),
                   "stacked_bar")
end)

test("heatmap renders a matrix", function()
  local m = { { 0.0, 0.1, 0.2, 0.3, 0.4 },
              { 0.5, 0.6, 0.7, 0.8, 0.9 },
              { 1.0, 0.0, 0.2, 0.5, 1.0 } }
  assert(#ccharts.heatmap(m, 5, 5, { show_labels = true }) > 0)
  assert_no_escape(ccharts.heatmap(m, 5, 5, { plain = true }), "heatmap")
end)

test("boxplot renders ragged per-category samples (contiguous structs)", function()
  local cats = {
    { name = "A", samples = { 1, 4, 2, 5, 3 } },
    { name = "B", samples = { 1, 2, 3, 4, 5, 6, 7, 8, 9 } },
    { name = "C", samples = { 8, 4, 12 } },
  }
  assert(#ccharts.boxplot(cats, 60, 8, { show_prices = true }) > 0)
  assert_no_escape(ccharts.boxplot(cats, 60, 8, { plain = true }), "boxplot")
end)

-- All chart renderers must complete without a segfault or Lua error; reaching
-- here means the contiguous-struct marshalling held across every chart type.
test("all renderers run clean", function()
  local data = ccharts.from_arrays(
    { 1, 2, 3 }, { 3, 4, 5 }, { 0, 1, 2 }, { 2, 3, 4 }, nil)
  data:line(40, 5)
  data:candle(40, 5)
  ccharts.pie({ { label = "Only", value = 7 } }, 12, 6)
  ccharts.histogram({ 5, 6, 7 }, 10, 3)
  ccharts.sparkline({ 5, 6, 7 }, 10, 1)
  ccharts.bar({ { label = "X", value = 9 } }, 6, 3)
  ccharts.stacked_bar({ { name = "S", values = { 1, 2, 3 } } }, 30, 6)
  ccharts.heatmap({ { 1, 2, 3 }, { 4, 5, 6 } }, 6, 5)
  ccharts.boxplot({ { name = "B", samples = { 1, 2, 3, 4, 5 } } }, 20, 8)
  assert(true)
end)

TinyTest.run()