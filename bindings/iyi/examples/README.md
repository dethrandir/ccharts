# A tiny financial chart CLI in iyi

`market.iyi` consumes the JSON emitted by the Florence `fl` CLI. It does not execute commands or fetch data itself. The program parses JSON with iyi's own `std/json`, checks the schema, parses ISO8601 timestamps with `std/time`, and passes typed columns into `Chart.from_arrays`. All chart rendering remains in ccharts.

After building the native library (see the binding README), from the repository root:

```sh
IYI_LIBRARY_PATH="$PWD/bindings/iyi/build" iyi build bindings/iyi/examples/market.iyi -o bindings/iyi/build/ccharts-market

fl market history THYAO 1m 1d --json | bindings/iyi/build/ccharts-market
fl market history THYAO 1m 1d --json | bindings/iyi/build/ccharts-market --type line
fl market history THYAO 1m 1d --json | bindings/iyi/build/ccharts-market --type spark --width 40
fl price ASELS --json | bindings/iyi/build/ccharts-market --type line --height 8

bindings/iyi/build/ccharts-market bindings/iyi/examples/fixtures/history.json --width 40
python3 bindings/iyi/examples/test_market.py bindings/iyi/build/ccharts-market
```

History input is a nonempty array of objects containing numeric `open`, `high`, `low`, `close` and `ts` (ISO8601 string or integer epoch seconds). Extra fields such as volume are ignored. Quote input contains numeric `previous_close`, `price`, and timestamps `previous_close_as_of`, `as_of`. A quote becomes two flat OHLC observations; it is not an invented intraday history. The fixtures are synthetic and need no account, network, or external Python package.

`--type candle|line|spark`, `--width N`, `--height N`, `--plain` (no ANSI colors), `--help` are supported. Spark renders one row. Dimensions must be 1–4096 and also satisfy the native renderer's allocation limits. Errors go to stderr with status 2. Use `set -o pipefail` in a shell script so an upstream `fl` failure also fails the pipeline.
