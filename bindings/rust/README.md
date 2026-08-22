# ccharts

Terminal charts for financial OHLC data — line and candlestick charts rendered
as ANSI-colored strings of Unicode block characters, ready to print.

Rust bindings for the C library [ccharts](https://github.com/dethrandir/ccharts).
The C sources are vendored and compiled by `build.rs`, so there is no system
dependency and nothing to install beyond a C compiler.

```toml
[dependencies]
ccharts = "0.2"
```

```rust
use ccharts::{Chart, Color, Settings};

let chart = Chart::from_arrays(&opens, &highs, &lows, &closes, Some(&epoch_seconds))?;

let settings = Settings::new()
    .rise(Color::Blue)
    .area(Color::BrightBlack)
    .show_prices(true)
    .show_times(true);

println!("{}", chart.line(60, 8, &settings)?);
println!("{}", chart.candle(60, 8, &Settings::new())?);
# Ok::<(), ccharts::Error>(())
```

- **Line charts** use eight vertical levels per cell (`▁▂▃▄▅▆▇█`) for smooth curves.
- **Candlestick charts** draw solid bodies between open/close and thin wicks
  (`│`) between high/low, downsampling automatically when there are more
  candles than columns.
- Data can also come from `Chart::from_json` (fixed-schema JSON) or
  `Chart::from_csv`.
- Colors are `Color` variants or raw escape sequences via `rise_ansi` and
  friends, so 256-color and truecolor work.

A `Chart` is immutable once built and is `Send + Sync`.

## License

MIT — see the [repository](https://github.com/dethrandir/ccharts).
