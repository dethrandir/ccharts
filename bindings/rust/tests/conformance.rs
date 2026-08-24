//! Renders the shared conformance cases and compares them byte for byte with
//! conformance/golden/*.txt, the contract every ccharts binding is held to.
//!
//! The suite lives at the repository root, outside the crate, so a published
//! crate simply skips it.

use std::path::{Path, PathBuf};

use ccharts::{Chart, Color, PieOptions, PieSlice, Settings};
use serde_json::Value;

fn suite_dir() -> Option<PathBuf> {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../conformance");
    if dir.join("cases.json").exists() {
        Some(dir)
    } else {
        None
    }
}

fn color(name: &Value) -> Option<Color> {
    let name = name.as_str()?;
    Some(match name {
        "black" => Color::Black,
        "red" => Color::Red,
        "green" => Color::Green,
        "yellow" => Color::Yellow,
        "blue" => Color::Blue,
        "magenta" => Color::Magenta,
        "cyan" => Color::Cyan,
        "white" => Color::White,
        "bright_black" => Color::BrightBlack,
        "bright_red" => Color::BrightRed,
        "bright_green" => Color::BrightGreen,
        "bright_yellow" => Color::BrightYellow,
        "bright_blue" => Color::BrightBlue,
        "bright_magenta" => Color::BrightMagenta,
        "bright_cyan" => Color::BrightCyan,
        "bright_white" => Color::BrightWhite,
        "reset" => Color::Reset,
        other => panic!("unknown color in cases.json: {other}"),
    })
}

fn floats(dataset: &Value, key: &str) -> Vec<f64> {
    dataset[key]
        .as_array()
        .expect("column")
        .iter()
        .map(|v| v.as_f64().expect("number"))
        .collect()
}

#[test]
fn matches_the_shared_goldens() {
    let dir = match suite_dir() {
        Some(dir) => dir,
        None => {
            eprintln!("conformance suite not present (published crate); skipping");
            return;
        }
    };

    let doc: Value =
        serde_json::from_slice(&std::fs::read(dir.join("cases.json")).unwrap()).unwrap();
    let cases = doc["cases"].as_array().expect("cases");
    assert!(cases.len() >= 10, "conformance suite looks truncated");

    let mut failures = Vec::new();
    for case in cases {
        let name = case["name"].as_str().unwrap();
        let width = case["width"].as_u64().unwrap() as u32;
        let height = case["height"].as_u64().unwrap() as u32;
        let cfg = &case["settings"];

        let rendered = if case["chart"].as_str().unwrap() == "pie" {
            let slices: Vec<PieSlice> = case["slices"]
                .as_array()
                .expect("pie slices")
                .iter()
                .map(|s| PieSlice {
                    label: s["label"].as_str(),
                    value: s["value"].as_f64().expect("slice value"),
                })
                .collect();
            let mut options = PieOptions::new()
                .donut(cfg["donut"].as_bool().unwrap_or(false))
                .show_legend(cfg["show_legend"].as_bool().unwrap_or(true))
                .show_pct(cfg["show_pct"].as_bool().unwrap_or(false));
            if let Some(colors) = cfg["colors"].as_array() {
                let named: Vec<Color> = colors
                    .iter()
                    .map(|c| color(c).expect("named pie color"))
                    .collect();
                options = options.colors(&named);
            }
            Chart::pie(&slices, width, height, &options)
        } else {
            let dataset = &doc["datasets"][case["dataset"].as_str().unwrap()];
            let chart = if case["source"] == "json" {
                Chart::from_json(dataset["json"].as_str().unwrap()).unwrap()
            } else {
                let ts: Option<Vec<i64>> = dataset
                    .get("ts")
                    .and_then(|t| t.as_array())
                    .map(|a| a.iter().map(|v| v.as_i64().expect("epoch")).collect());
                Chart::from_arrays(
                    &floats(dataset, "open"),
                    &floats(dataset, "high"),
                    &floats(dataset, "low"),
                    &floats(dataset, "close"),
                    ts.as_deref(),
                )
                .unwrap()
            };

            let mut settings = Settings::new()
                .single_color(cfg["single_color"].as_bool().unwrap())
                .show_prices(cfg["show_prices"].as_bool().unwrap())
                .show_times(cfg["show_times"].as_bool().unwrap())
                .plain(cfg["plain"].as_bool().unwrap_or(false));
            if let Some(c) = color(&cfg["rise_color"]) {
                settings = settings.rise(c);
            }
            if let Some(c) = color(&cfg["fall_color"]) {
                settings = settings.fall(c);
            }
            if let Some(c) = color(&cfg["bg_color"]) {
                settings = settings.background(c);
            }
            if let Some(c) = color(&cfg["area_color"]) {
                settings = settings.area(c);
            }

            match case["chart"].as_str().unwrap() {
                "line" => chart.line(width, height, &settings),
                other => {
                    assert_eq!(other, "candle");
                    chart.candle(width, height, &settings)
                }
            }
        }
        .unwrap();

        let golden = dir.join("golden").join(format!("{name}.txt"));
        let expected =
            std::fs::read(&golden).unwrap_or_else(|_| panic!("missing golden file for {name}"));
        if rendered.as_bytes() != expected.as_slice() {
            failures.push(name.to_string());
        }
    }

    assert!(
        failures.is_empty(),
        "cases differ from the goldens: {failures:?}"
    );
}
