//! Behavioral tests for the safe wrapper. The rendering itself is checked
//! against the shared goldens in conformance.rs.

use ccharts::{
    BarOptions, Chart, Color, Error, HistogramOptions, PieOptions, PieSlice, Settings,
    SparklineOptions, StackOptions,
};

fn sample() -> Chart {
    Chart::from_arrays(
        &[328.75, 330.0, 317.25, 320.0, 306.0],
        &[330.0, 330.25, 321.0, 328.75, 307.25],
        &[323.75, 317.5, 314.5, 317.75, 300.75],
        &[328.0, 317.5, 321.0, 318.0, 301.0],
        Some(&[1784505600, 1784592000, 1784678400, 1784764800, 1784851200]),
    )
    .expect("dataset")
}

#[test]
fn renders_both_chart_types() {
    let chart = sample();
    assert_eq!(chart.len(), 5);
    let line = chart.line(40, 4, &Settings::new()).unwrap();
    let candle = chart.candle(40, 4, &Settings::new()).unwrap();
    assert_eq!(line.lines().count(), 4);
    assert!(!candle.is_empty());
    assert!(candle.contains('\u{2502}'), "candles draw wicks");
}

#[test]
fn json_and_arrays_agree() {
    let json = r#"[{"ts":"2026-07-20T00:00:00+00:00","open":328.75,"high":330.0,"low":323.75,"close":328.0},
                   {"ts":"2026-07-21T00:00:00+00:00","open":330.0,"high":330.25,"low":317.5,"close":317.5},
                   {"ts":"2026-07-22T00:00:00+00:00","open":317.25,"high":321.0,"low":314.5,"close":321.0},
                   {"ts":"2026-07-23T00:00:00+00:00","open":320.0,"high":328.75,"low":317.75,"close":318.0},
                   {"ts":"2026-07-24T00:00:00+00:00","open":306.0,"high":307.25,"low":300.75,"close":301.0}]"#;
    let settings = Settings::new().show_prices(true).show_times(true);
    let from_json = Chart::from_json(json).unwrap();
    assert_eq!(
        from_json.line(60, 8, &settings).unwrap(),
        sample().line(60, 8, &settings).unwrap()
    );
}

#[test]
fn csv_skips_blank_lines() {
    let chart = Chart::from_csv("1,2,0.5,1.5\n\n   \n2,3,1,2.5\n", b',', b'\n').unwrap();
    assert_eq!(chart.len(), 2, "blank lines must not become zeroed candles");
}

#[test]
fn colors_come_from_the_c_library() {
    assert_eq!(Color::Blue.as_str(), "\x1b[34m");
    assert_eq!(Color::BrightWhite.as_str(), "\x1b[97m");
    let out = sample()
        .line(40, 4, &Settings::new().rise(Color::Blue).fall(Color::Red))
        .unwrap();
    assert!(out.contains("\x1b[34m") && out.contains("\x1b[31m"));
}

#[test]
fn custom_escapes_are_passed_through() {
    let out = sample()
        .line(40, 4, &Settings::new().rise_ansi("\x1b[38;5;208m"))
        .unwrap();
    assert!(out.contains("\x1b[38;5;208m"), "truecolor/256 escapes work");
}

#[test]
fn plain_output_has_no_escapes() {
    let chart = sample();
    let settings = Settings::new()
        .plain(true)
        .rise(Color::Blue)
        .area(Color::BrightBlack)
        .show_prices(true)
        .show_times(true);
    let line = chart.line(40, 5, &settings).unwrap();
    let candle = chart.candle(40, 5, &settings).unwrap();
    assert!(!line.contains('\u{1b}'), "plain must override every color");
    assert!(!candle.contains('\u{1b}'));
    assert!(line.contains("2026-07-20"), "labels are still drawn");
    // Without plain, the same settings do color the output.
    let colored = chart
        .line(40, 5, &Settings::new().rise(Color::Blue))
        .unwrap();
    assert!(colored.contains('\u{1b}'));
}

#[test]
fn mismatched_columns_are_rejected() {
    let err = Chart::from_arrays(&[1.0, 2.0], &[2.0], &[0.5, 1.0], &[1.5, 2.0], None).unwrap_err();
    assert!(matches!(err, Error::InvalidArgument(_)));
}

#[test]
fn empty_input_is_rejected() {
    assert!(matches!(
        Chart::from_arrays(&[], &[], &[], &[], None),
        Err(Error::InvalidArgument(_))
    ));
}

#[test]
fn non_finite_prices_are_rejected() {
    for bad in [f64::NAN, f64::INFINITY, f64::NEG_INFINITY] {
        assert_eq!(
            Chart::from_arrays(&[bad], &[2.0], &[0.5], &[1.5], None).unwrap_err(),
            Error::NonFinite
        );
    }
}

#[test]
fn bad_dimensions_are_an_error_not_an_empty_string() {
    let chart = sample();
    assert_eq!(
        chart.line(0, 5, &Settings::new()).unwrap_err(),
        Error::Dimensions
    );
    assert_eq!(
        chart.candle(5, 0, &Settings::new()).unwrap_err(),
        Error::Dimensions
    );
    assert_eq!(
        chart.line(1000, 2000, &Settings::new()).unwrap_err(),
        Error::Dimensions
    );
}

#[test]
fn malformed_json_is_rejected() {
    assert_eq!(Chart::from_json("not json").unwrap_err(), Error::Parse);
    assert_eq!(Chart::from_json("[]").unwrap_err(), Error::Parse);
}

#[test]
fn interior_nul_is_rejected() {
    assert_eq!(Chart::from_json("[{\0}]").unwrap_err(), Error::InteriorNul);
}

#[test]
fn charts_are_shareable_across_threads() {
    let chart = std::sync::Arc::new(sample());
    let handles: Vec<_> = (0..4)
        .map(|_| {
            let chart = std::sync::Arc::clone(&chart);
            std::thread::spawn(move || chart.line(30, 4, &Settings::new()).unwrap())
        })
        .collect();
    let outputs: Vec<String> = handles.into_iter().map(|h| h.join().unwrap()).collect();
    assert!(outputs.windows(2).all(|w| w[0] == w[1]));
}

#[test]
fn exposes_library_metadata() {
    assert_eq!(ccharts::version(), "0.2.2");
    assert_eq!(ccharts::max_dim(), 100_000);
    assert_eq!(ccharts::max_cells(), 1_000_000);
}

#[test]
fn pie_renders_disk_and_donut_and_rejects_bad_input() {
    let slices = [PieSlice { label: Some("A"), value: 40.0 },
                  PieSlice { label: Some("B"), value: 30.0 },
                  PieSlice { label: Some("C"), value: 30.0 }];

    let disk = Chart::pie(&slices, 24, 10, &PieOptions::new()).unwrap();
    assert!(disk.contains('█'));

    let donut = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true)).unwrap();
    assert_ne!(disk, donut);

    // All-zero slices render the empty string, not an error.
    let empty = Chart::pie(&[PieSlice { label: Some("Zero"), value: 0.0 }],
                           24, 10, &PieOptions::new()).unwrap();
    assert!(empty.is_empty());

    // NaN and inf are rejected with Error::NonFinite.
    let bad = Chart::pie(&[PieSlice { label: None, value: f64::NAN }],
                         24, 10, &PieOptions::new());
    assert_eq!(bad.unwrap_err(), ccharts::Error::NonFinite);
}

#[test]
fn pie_optional_settings_are_wired_through() {
    let slices = [PieSlice { label: Some("A"), value: 40.0 },
                  PieSlice { label: Some("B"), value: 30.0 },
                  PieSlice { label: Some("C"), value: 30.0 }];

    let base = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true)).unwrap();

    let gap = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true).slice_gap(0.15)).unwrap();
    let thick = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true)
        .inner_radius_ratio(0.2)).unwrap();
    let alt = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true)
        .legend_format(1)).unwrap();
    let angled = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true)
        .start_angle(0.0)).unwrap();
    let cw = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true)
        .counter_clockwise(true)).unwrap();
    let centered = Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true)
        .center_text("42")).unwrap();

    // Every new option changes at least the render, proving the value reached
    // the renderer rather than being ignored.
    assert_ne!(base, gap, "slice_gap must take effect");
    assert_ne!(base, thick, "inner_radius_ratio must take effect");
    assert_ne!(base, alt, "legend_format must take effect");
    assert_ne!(base, angled, "start_angle must take effect");
    assert_ne!(base, cw, "counter_clockwise must take effect");
    assert_ne!(base, centered, "center_text must take effect");

    // An interior NUL in the center text is rejected.
    let err = Chart::pie(&slices, 24, 10, &PieOptions::new().center_text("a\0b")).unwrap_err();
    assert_eq!(err, ccharts::Error::InteriorNul);
}

#[test]
fn histogram_renders_and_wires_options() {
    let samples = [0.0, 1.0, 1.0, 2.0, 2.0, 2.0, 3.0, 3.0, 4.0, 4.0, 5.0];
    let base = Chart::histogram(&samples, 40, 6, &HistogramOptions::new()).unwrap();
    assert!(base.contains('█'), "bar glyphs present");
    assert_eq!(base.lines().count(), 6);

    // A >0 bin count and an explicit range each change the render.
    let binned =
        Chart::histogram(&samples, 40, 6, &HistogramOptions::new().bin_count(5)).unwrap();
    assert_ne!(base, binned, "bin_count must take effect");

    let ranged = Chart::histogram(
        &samples,
        40,
        6,
        &HistogramOptions::new().min_value(-2.0).max_value(8.0),
    )
    .unwrap();
    assert_ne!(base, ranged, "min/max range must take effect");

    let margined =
        Chart::histogram(&samples, 40, 6, &HistogramOptions::new().show_prices(true)).unwrap();
    assert_ne!(base, margined, "show_prices must take effect");

    let footed =
        Chart::histogram(&samples, 40, 6, &HistogramOptions::new().show_bins(true)).unwrap();
    assert_ne!(base, footed, "show_bins must take effect");

    // NaN/inf samples are rejected with Error::NonFinite.
    for bad in [f64::NAN, f64::INFINITY] {
        assert_eq!(
            Chart::histogram(&[bad], 40, 6, &HistogramOptions::new()).unwrap_err(),
            ccharts::Error::NonFinite
        );
    }

    // Colors flow through.
    let colored = Chart::histogram(&samples, 40, 6, &HistogramOptions::new().rise(Color::Blue))
        .unwrap();
    assert!(colored.contains("\u{1b}[34m"));
}

#[test]
fn sparkline_renders_and_wires_options() {
    let samples = [5.0, 7.0, 4.0, 8.0, 6.0, 9.0, 4.0, 7.0, 10.0, 8.0];
    let base = Chart::sparkline(&samples, 24, 1, &SparklineOptions::new()).unwrap();
    assert!(!base.is_empty(), "single-row sparkline renders");

    // Reserved edge sub-pixels change the render of a tiny height.
    let margined = Chart::sparkline(
        &samples,
        24,
        1,
        &SparklineOptions::new().min_above(2).min_below(1),
    )
    .unwrap();
    assert_ne!(base, margined, "min_above/min_below must take effect");

    // An area color changes the render.
    let area =
        Chart::sparkline(&samples, 24, 2, &SparklineOptions::new().area(Color::BrightBlack))
            .unwrap();
    assert_ne!(base, area, "area_color must take effect");

    // NaN/inf samples are rejected with Error::NonFinite.
    for bad in [f64::NAN, f64::INFINITY] {
        assert_eq!(
            Chart::sparkline(&[bad], 24, 1, &SparklineOptions::new()).unwrap_err(),
            Error::NonFinite
        );
    }

    // Colors flow through.
    let colored = Chart::sparkline(&samples, 24, 1, &SparklineOptions::new().rise(Color::Blue))
        .unwrap();
    assert!(colored.contains("\u{1b}[34m"));
}

#[test]
fn bar_renders_and_wires_options() {
    let labels = ["Mon", "Tue", "Wed"];
    let values = [2.0, 5.0, 3.0];
    let base = Chart::bar(&labels, &values, 12, 6, &BarOptions::new()).unwrap();
    assert!(base.contains('█'), "bar glyphs present");
    assert_eq!(base.lines().count(), 6);

    // A label footer changes the render.
    let labeled = Chart::bar(&labels, &values, 12, 6, &BarOptions::new().show_labels(true)).unwrap();
    assert_ne!(base, labeled, "show_labels must take effect");

    // A value axis changes the render.
    let margined = Chart::bar(&labels, &values, 12, 6, &BarOptions::new().show_prices(true)).unwrap();
    assert_ne!(base, margined, "show_prices must take effect");

    // Plain output has no escapes.
    let plain = Chart::bar(&labels, &values, 12, 6, &BarOptions::new().plain(true)).unwrap();
    assert!(!plain.contains('\u{1b}'), "plain must override every color");

    // NaN/inf values are rejected with Error::NonFinite.
    for bad in [f64::NAN, f64::INFINITY, f64::NEG_INFINITY] {
        assert_eq!(
            Chart::bar(&["A"], &[bad], 12, 6, &BarOptions::new()).unwrap_err(),
            Error::NonFinite
        );
    }

    // Mismatched label/value lengths are rejected.
    assert!(matches!(
        Chart::bar(&["A", "B"], &[1.0], 12, 6, &BarOptions::new()).unwrap_err(),
        Error::InvalidArgument(_)
    ));

    // Colors flow through.
    let colored = Chart::bar(&labels, &values, 12, 6, &BarOptions::new().rise(Color::Blue)).unwrap();
    assert!(colored.contains("\u{1b}[34m"));

    // An interior NUL in a label is rejected.
    assert_eq!(
        Chart::bar(&["a\0b"], &[1.0], 12, 6, &BarOptions::new()).unwrap_err(),
        Error::InteriorNul
    );
}

#[test]
fn stacked_bar_renders_and_wires_options() {
    let series = [
        ("Alpha", &[1.0f64, 4.0, 2.0, 5.0, 3.0][..]),
        ("Beta", &[3.0, 2.0, 5.0, 1.0, 4.0][..]),
    ];
    let base = Chart::stacked_bar(&series, 20, 8, &StackOptions::new()).unwrap();
    assert!(base.contains('█'), "stack glyphs present");
    assert_eq!(base.lines().count(), 8);

    // A category-label footer changes the render.
    let labeled = Chart::stacked_bar(&series, 20, 8, &StackOptions::new().show_labels(true)).unwrap();
    assert_ne!(base, labeled, "show_labels must take effect");

    // Category names flow through the footer.
    let named = Chart::stacked_bar(
        &series,
        20,
        8,
        &StackOptions::new()
            .show_labels(true)
            .category_labels(&["Jan", "Feb", "Mar", "Apr", "May"]),
    )
    .unwrap();
    assert_ne!(named, labeled, "category labels must reach the render");

    // A value axis changes the render.
    let margined =
        Chart::stacked_bar(&series, 20, 8, &StackOptions::new().show_prices(true)).unwrap();
    assert_ne!(base, margined, "show_prices must take effect");

    // Plain output has no escapes.
    let plain = Chart::stacked_bar(&series, 20, 8, &StackOptions::new().plain(true)).unwrap();
    assert!(!plain.contains('\u{1b}'), "plain must override every color");

    // NaN/inf values are rejected with Error::NonFinite.
    for bad in [f64::NAN, f64::INFINITY, f64::NEG_INFINITY] {
        let bad_series = [("A", &[bad][..]), ("B", &[1.0][..])];
        assert_eq!(
            Chart::stacked_bar(&bad_series, 20, 8, &StackOptions::new()).unwrap_err(),
            Error::NonFinite
        );
    }

    // Unequal series lengths are rejected.
    let unequal = [("A", &[1.0, 2.0][..]), ("B", &[1.0][..])];
    assert!(matches!(
        Chart::stacked_bar(&unequal, 20, 8, &StackOptions::new()).unwrap_err(),
        Error::InvalidArgument(_)
    ));

    // Colors flow through.
    let colored =
        Chart::stacked_bar(&series, 20, 8, &StackOptions::new().colors(&[Color::Red, Color::Green]))
            .unwrap();
    assert!(colored.contains("\u{1b}[31m"));
}
