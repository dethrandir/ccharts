# frozen_string_literal: true

require_relative "helper"

# Basic API shape and error-contract tests. The byte-for-byte rendering is
# covered by test_conformance.rb.
test "library version matches the gem VERSION" do
  assert_equal Ccharts::VERSION, Ccharts.version
  assert_equal "3.0.0", Ccharts.version
end

test "max_dim and max_cells are positive" do
  assert Ccharts.max_dim > 0
  assert Ccharts.max_cells > 0
end

test "color names resolve to ANSI escapes" do
  assert_equal "\e[34m", Ccharts::Color["blue"]
  assert_equal "\e[32m", Ccharts::Color["green"]
  assert_equal "\e[0m", Ccharts::Color["reset"]
end

open  = [328.75, 330.0, 317.25]
high  = [330.0, 330.25, 321.0]
low   = [323.75, 317.5, 314.5]
close = [328.0, 317.5, 321.0]

def from_arrays_sample
  Ccharts::Chart.from_arrays(open: [1.0, 2.0], high: [3.0, 4.0], low: [0.5, 1.0], close: [1.5, 3.0])
end

test "from_arrays builds a dataset with the right length" do
  c = Ccharts::Chart.from_arrays(open: open, high: high, low: low, close: close)
  assert_equal 3, c.size
end

test "line and candle render a string" do
  c = Ccharts::Chart.from_arrays(open: open, high: high, low: low, close: close)
  assert c.line(40, 8).is_a?(String)
  assert c.line(40, 8).bytesize > 0
  assert c.candle(40, 8).is_a?(String)
  assert c.candle(40, 8).bytesize > 0
end

test "from_json parses" do
  json = '[{"open":1.0,"high":2.0,"low":0.5,"close":1.5}]'
  c = Ccharts::Chart.from_json(json)
  assert_equal 1, c.size
end

test "from_csv parses" do
  csv = "1.0,2.0,0.5,1.5\n2.0,3.0,1.0,2.5\n"
  c = Ccharts::Chart.from_csv(csv)
  assert_equal 2, c.size
end

test "all standalone renderers return a non-empty string" do
  chart = Ccharts::Chart
  assert chart.pie([{ label: "A", value: 40 }, { label: "B", value: 60 }], 24, 10).is_a?(String)
  assert chart.histogram([1, 2, 2, 3, 3, 3, 4], 30, 8).is_a?(String)
  assert chart.sparkline([1, 2, 3, 2, 4, 5], 20, 1).is_a?(String)
  assert chart.bar(%w[A B C], [1, 4, 2], 20, 8).is_a?(String)
  assert chart.stacked_bar(
    [{ name: "Alpha", values: [1, 4, 2] }, { name: "Beta", values: [3, 2, 5] }],
    20, 8
  ).is_a?(String)
  assert chart.heatmap([[0.0, 0.5, 1.0], [0.2, 0.8, 0.0]], 6, 6).is_a?(String)
  assert chart.boxplot(
    [{ name: "A", samples: [1, 2, 3, 4, 5] }, { name: "B", samples: [9, 8, 7] }],
    30, 8
  ).is_a?(String)
end

test "non-finite values raise Ccharts::Error (histogram)" do
  assert_raises(Ccharts::Error) do
    Ccharts::Chart.histogram([1.0, Float::NAN], 20, 5)
  end
end

test "non-finite values raise in from_arrays" do
  assert_raises(Ccharts::Error) do
    Ccharts::Chart.from_arrays(open: [1.0, Float::INFINITY], high: [2.0, 3.0],
                               low: [0.5, 1.0], close: [1.5, 2.0])
  end
end

test "invalid dimensions raise" do
  assert_raises(Ccharts::Error) do
    Ccharts::Chart.histogram([1.0], 0, 5)
  end
end

test "settings builders: plain suppresses ANSI" do
  c = Ccharts::Chart.from_arrays(open: open, high: high, low: low, close: close)
  colored = c.line(40, 8)
  plain = c.line(40, 8, Ccharts::Settings::ChartSettings.new.plain(true))
  assert colored.include?("\e[")
  refute(plain.include?("\e["))
end

def refute(cond, msg = "expected false")
  raise msg if cond
end

test "hundreds of renders do not leak or crash" do
  c = Ccharts::Chart.from_arrays(open: open, high: high, low: low, close: close)
  200.times do
    c.line(40, 8)
    Ccharts::Chart.pie([{ label: "A", value: 1 }, { label: "B", value: 2 }], 20, 10)
  end
  assert true
end

def from_arrays_sample_scope; end

TinyTest.run
