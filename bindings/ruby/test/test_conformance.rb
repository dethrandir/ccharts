# frozen_string_literal: true

require_relative "helper"

# The shared 70-case conformance suite: render every case in
# conformance/cases.json and compare byte-for-byte against the goldens in
# conformance/golden/<name>.txt — the contract every ccharts binding is held to.
#
# A settings/struct mismatch in the Fiddle layer shows up here as a byte
# difference, not a crash, so this is the acceptance gate for the binding.

def color(value)
  value # a color name string like "blue", or nil
end

def floats(dataset, key)
  dataset[key].map { |v| v.to_f }
end

def render_case(cases, case_data)
  cfg = case_data["settings"] || {}
  case case_data["chart"]
  when "hist"
    samples = case_data["samples"].map { |v| v.to_f }
    s = Ccharts::Settings::HistSettings.new
    s.bin_count((cfg["bin_count"] || 0).to_i)
    s.min_value(cfg["min_value"].nil? ? Float::NAN : cfg["min_value"].to_f)
    s.max_value(cfg["max_value"].nil? ? Float::NAN : cfg["max_value"].to_f)
    s.show_bins(cfg["show_bins"] || false)
    s.show_prices(cfg["show_prices"] || false)
    s.plain(cfg["plain"] || false)
    s.rise(color(cfg["rise_color"])) if cfg["rise_color"]
    s.background(color(cfg["bg_color"])) if cfg["bg_color"]
    Ccharts::Chart.histogram(samples, case_data["width"], case_data["height"], s)

  when "spark"
    samples = case_data["samples"].map { |v| v.to_f }
    s = Ccharts::Settings::SparkSettings.new
    s.min_above((cfg["min_above"] || 0).to_i)
    s.min_below((cfg["min_below"] || 0).to_i)
    s.plain(cfg["plain"] || false)
    s.rise(color(cfg["rise_color"])) if cfg["rise_color"]
    s.area(color(cfg["area_color"])) if cfg["area_color"]
    Ccharts::Chart.sparkline(samples, case_data["width"], case_data["height"], s)

  when "bar"
    labels = case_data["items"].map { |it| it["label"].to_s }
    values = case_data["items"].map { |it| it["value"].to_f }
    s = Ccharts::Settings::BarSettings.new
    s.show_labels(cfg["show_labels"] || false)
    s.show_prices(cfg["show_prices"] || false)
    s.plain(cfg["plain"] || false)
    s.rise(color(cfg["rise_color"])) if cfg["rise_color"]
    s.background(color(cfg["bg_color"])) if cfg["bg_color"]
    Ccharts::Chart.bar(labels, values, case_data["width"], case_data["height"], s)

  when "pie"
    slices = case_data["slices"].map { |sl| { label: sl["label"].to_s, value: sl["value"].to_f } }
    s = Ccharts::Settings::PieSettings.new
    s.donut(cfg["donut"] || false)
    s.show_legend(cfg["show_legend"].nil? ? true : cfg["show_legend"])
    s.show_pct(cfg["show_pct"] || false)
    s.slice_gap(cfg["slice_gap"].to_f) if cfg["slice_gap"]
    s.inner_radius_ratio(cfg["inner_radius_ratio"].to_f) if cfg["inner_radius_ratio"]
    s.legend_format(cfg["legend_format"].to_i) if cfg["legend_format"]
    s.start_angle(cfg["start_angle"].to_f) if cfg["start_angle"]
    s.counter_clockwise(cfg["counter_clockwise"] || false)
    s.center_text(cfg["center_text"]) if cfg["center_text"]
    s.colors(cfg["colors"]) if cfg["colors"]
    Ccharts::Chart.pie(slices, case_data["width"], case_data["height"], s)

  when "stack"
    series = case_data["series"].map do |sr|
      { name: sr["name"].to_s, values: sr["values"].map { |v| v.to_f } }
    end
    s = Ccharts::Settings::StackSettings.new
    s.show_labels(cfg["show_labels"] || false)
    s.show_prices(cfg["show_prices"] || false)
    s.plain(cfg["plain"] || false)
    s.colors(cfg["colors"]) if cfg["colors"]
    s.background(color(cfg["bg_color"])) if cfg["bg_color"]
    s.category_labels(cfg["cat_labels"]) if cfg["cat_labels"]
    Ccharts::Chart.stacked_bar(series, case_data["width"], case_data["height"], s)

  when "heat"
    matrix = case_data["values"].map { |row| row.map { |v| v.to_f } }
    s = Ccharts::Settings::HeatSettings.new
    s.show_labels(cfg["show_labels"] || false)
    s.plain(cfg["plain"] || false)
    s.low_color(color(cfg["low_color"])) if cfg["low_color"]
    s.high_color(color(cfg["high_color"])) if cfg["high_color"]
    s.mid_color(color(cfg["mid_color"])) if cfg["mid_color"]
    s.background(color(cfg["bg_color"])) if cfg["bg_color"]
    s.row_labels(cfg["row_labels"]) if cfg["row_labels"]
    s.col_labels(cfg["col_labels"]) if cfg["col_labels"]
    Ccharts::Chart.heatmap(matrix, case_data["width"], case_data["height"], s)

  when "box"
    categories = case_data["categories"].map do |c|
      { name: c["name"].to_s, samples: c["samples"].map { |v| v.to_f } }
    end
    s = Ccharts::Settings::BoxSettings.new
    s.show_prices(cfg["show_prices"] || false)
    s.plain(cfg["plain"] || false)
    s.rise(color(cfg["rise_color"])) if cfg["rise_color"]
    s.area(color(cfg["area_color"])) if cfg["area_color"]
    s.background(color(cfg["bg_color"])) if cfg["bg_color"]
    Ccharts::Chart.boxplot(categories, case_data["width"], case_data["height"], s)

  else # line / candle
    dataset = cases["datasets"][case_data["dataset"]]
    chart = if case_data["source"] == "json"
              Ccharts::Chart.from_json(dataset["json"])
            else
              ts = dataset["ts"] ? dataset["ts"].map(&:to_i) : nil
              Ccharts::Chart.from_arrays(
                open: floats(dataset, "open"), high: floats(dataset, "high"),
                low: floats(dataset, "low"), close: floats(dataset, "close"), ts: ts
              )
            end
    s = Ccharts::Settings::ChartSettings.new
    s.single_color(cfg["single_color"] || false)
    s.show_prices(cfg["show_prices"] || false)
    s.show_times(cfg["show_times"] || false)
    s.plain(cfg["plain"] || false)
    s.rise(color(cfg["rise_color"])) if cfg["rise_color"]
    s.fall(color(cfg["fall_color"])) if cfg["fall_color"]
    s.background(color(cfg["bg_color"])) if cfg["bg_color"]
    s.area(color(cfg["area_color"])) if cfg["area_color"]
    if case_data["chart"] == "line"
      chart.line(case_data["width"], case_data["height"], s)
    else
      chart.candle(case_data["width"], case_data["height"], s)
    end
  end
end

test "all 70 conformance cases match the goldens byte-for-byte" do
  dir = conformance_dir
  raise "conformance suite not present at #{dir}" unless File.exist?(File.join(dir, "cases.json"))

  doc = JSON.parse(File.read(File.join(dir, "cases.json")))
  cases = doc["cases"]
  assert cases.length >= 70, "conformance suite looks truncated (#{cases.length} cases)"

  failures = []
  cases.each do |case_data|
    name = case_data["name"]
    rendered = render_case(doc, case_data)
    expected_path = File.join(dir, "golden", "#{name}.txt")
    raise "missing golden file for #{name}" unless File.exist?(expected_path)
    expected = File.binread(expected_path)
    unless rendered.b == expected
      failures << name
    end
  end

  unless failures.empty?
    raise "#{failures.length} case(s) differ from the goldens: #{failures.inspect}"
  end
  assert true
end

TinyTest.run
