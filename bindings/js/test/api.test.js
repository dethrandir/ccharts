import assert from "node:assert/strict";
import test from "node:test";

import { CchartsError, Chart, Color, maxCells, maxDim, version } from "../src/index.js";

const OPEN = [328.75, 330.0, 317.25, 320.0, 306.0];
const HIGH = [330.0, 330.25, 321.0, 328.75, 307.25];
const LOW = [323.75, 317.5, 314.5, 317.75, 300.75];
const CLOSE = [328.0, 317.5, 321.0, 318.0, 301.0];
const TS = [1784505600, 1784592000, 1784678400, 1784764800, 1784851200];

const sample = () => Chart.fromArrays(OPEN, HIGH, LOW, CLOSE, TS);

test("renders both chart types", () => {
  const chart = sample();
  assert.equal(chart.length, 5);
  assert.equal(chart.line({ width: 40, height: 4 }).trimEnd().split("\n").length, 4);
  assert.match(chart.candle({ width: 40, height: 4 }), /│/);
  chart.free();
});

test("the JSON and array entry points agree", () => {
  const document = JSON.stringify(
    OPEN.map((open, i) => ({
      ts: new Date(TS[i] * 1000).toISOString().replace(".000Z", "+00:00"),
      open, high: HIGH[i], low: LOW[i], close: CLOSE[i],
    })),
  );
  const options = { width: 60, height: 8, showPrices: true, showTimes: true };
  const fromJson = Chart.fromJson(document);
  const fromArrays = sample();
  assert.equal(fromJson.line(options), fromArrays.line(options));
  assert.equal(fromJson.candle(options), fromArrays.candle(options));
  fromJson.free();
  fromArrays.free();
});

test("typed arrays take the fast path and match plain arrays", () => {
  const typed = Chart.fromArrays(
    Float64Array.from(OPEN), Float64Array.from(HIGH),
    Float64Array.from(LOW), Float64Array.from(CLOSE),
    BigInt64Array.from(TS.map(BigInt)),
  );
  const plain = sample();
  assert.equal(typed.line({ showTimes: true }), plain.line({ showTimes: true }));
  typed.free();
  plain.free();
});

test("CSV skips blank lines instead of zero-filling candles", () => {
  const chart = Chart.fromCsv("1,2,0.5,1.5\n\n   \n2,3,1,2.5\n");
  assert.equal(chart.length, 2);
  chart.free();
});

test("colors come from the C library", () => {
  assert.equal(Color.blue, "\x1b[34m");
  assert.equal(Color.brightWhite, "\x1b[97m");
  const chart = sample();
  const out = chart.line({ width: 40, height: 4, riseColor: Color.blue, fallColor: Color.red });
  assert.ok(out.includes("\x1b[34m") && out.includes("\x1b[31m"));
  chart.free();
});

test("custom escapes pass through", () => {
  const chart = sample();
  assert.ok(chart.line({ riseColor: "\x1b[38;5;208m" }).includes("\x1b[38;5;208m"));
  chart.free();
});

test("invalid input is rejected", () => {
  const cases = [
    [() => Chart.fromArrays([], [], [], []), 1],
    [() => Chart.fromArrays([1, 2], [2], [0, 1], [1, 2]), 1],
    [() => Chart.fromArrays([1], [2], [0], [1], [1, 2]), 1],
    [() => Chart.fromArrays([NaN], [2], [0], [1]), 4],
    [() => Chart.fromArrays([Infinity], [2], [0], [1]), 4],
    [() => Chart.fromJson("not json"), 2],
    [() => Chart.fromJson("[]"), 2],
  ];
  for (const [fn, code] of cases) {
    assert.throws(fn, (err) => err instanceof CchartsError && err.code === code);
  }
});

test("bad dimensions are an error, not an empty string", () => {
  const chart = sample();
  for (const options of [{ width: 0 }, { height: 0 }, { width: -1 },
                         { width: 1000, height: 2000 }, { width: 1.5 }]) {
    assert.throws(() => chart.line(options),
      (err) => err instanceof CchartsError && err.code === 5);
  }
  chart.free();
});

test("free is idempotent and guards later renders", () => {
  const chart = sample();
  chart.free();
  chart.free();
  assert.throws(() => chart.line(), /freed/);
});

test("many charts do not exhaust wasm memory", () => {
  // Exercises malloc/free balance and the memory-growth path.
  for (let i = 0; i < 500; i++) {
    const chart = Chart.fromArrays(OPEN, HIGH, LOW, CLOSE, TS);
    chart.line({ width: 80, height: 10, showPrices: true, showTimes: true });
    chart.candle({ width: 80, height: 10 });
    chart.free();
  }
});

test("exposes library metadata", () => {
  assert.equal(version, "0.2.2");
  assert.equal(maxDim, 100000);
  assert.equal(maxCells, 1000000);
});

test("pie renders a disk, a donut, and the empty string for zero values", () => {
  const slices = [
    { label: "Alpha", value: 40 },
    { label: "Beta", value: 30 },
    { label: "Gamma", value: 30 },
  ];
  const disk = Chart.pie(slices, { width: 24, height: 10, showLegend: true, showPct: true });
  assert.ok(disk.includes("Alpha  40 (40%)"));
  const donut = Chart.pie(slices, { width: 24, height: 10, donut: true, showLegend: true });
  assert.notEqual(disk, donut);

  assert.equal(Chart.pie([{ label: "Zero", value: 0 }], {}), "");

  assert.throws(() => Chart.pie([{ label: "Bad", value: NaN }], {}),
    (err) => err instanceof CchartsError && err.code === 4);
  assert.throws(() => Chart.pie([], {}),
    (err) => err instanceof CchartsError && err.code === 1);
});
