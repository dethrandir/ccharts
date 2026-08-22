# ccharts

Turns financial OHLC data into a string — line and candlestick charts drawn
with Unicode block characters. Nothing is printed for you, so the chart goes
wherever text goes: a terminal, a log line, a chat message, an HTML `<pre>`,
a file.

This is the C library [ccharts](https://github.com/dethrandir/ccharts) compiled
to WebAssembly. One package works everywhere — Node, Deno, Bun and the browser —
with no native build step, no prebuilt binaries and **no dependencies**. The
`.wasm` is embedded in the JavaScript, so there is nothing to fetch and no
bundler configuration.

```sh
npm install ccharts
```

```js
import { Chart, Color } from "ccharts";

const chart = Chart.fromArrays(open, high, low, close, epochSeconds);

console.log(chart.line({
  width: 60, height: 8,
  riseColor: Color.blue,
  showPrices: true,
  showTimes: true,
}));
console.log(chart.candle({ width: 60, height: 8 }));

chart.free();
```

- Data can also come from `Chart.fromJson` (fixed-schema JSON) or
  `Chart.fromCsv`.
- `Color` holds the sixteen ANSI colors; any escape string works, so
  256-color and truecolor do too: `{ riseColor: "\x1b[38;5;208m" }`.
- `{ plain: true }` renders with no ANSI escapes at all, for output that is
  not going to a terminal.
- Plain arrays, `Float64Array` and `BigInt64Array` are all accepted.
- `free()` releases the WebAssembly memory. A `FinalizationRegistry` covers a
  forgotten call, but it runs whenever the GC feels like it.

Errors are `CchartsError` with a numeric `code` (1 invalid argument, 2 parse,
3 out of memory, 4 non-finite value, 5 bad dimensions).

## Notes

- **ESM only.** The package uses top-level await to instantiate the
  WebAssembly module at import time, so `import { Chart } from "ccharts"` needs
  no initialization call. There is no CommonJS build.
- **Node 18+** for `TextDecoder`, `FinalizationRegistry` and top-level await.
- Rebuilding the WebAssembly module (`npm run build`) needs
  [emscripten](https://emscripten.org); `src/wasm-binary.js` is checked in, so
  only contributors touching the C sources need it.

## License

MIT — see the [repository](https://github.com/dethrandir/ccharts).
