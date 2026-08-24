/**
 * Renders the shared conformance cases and compares them byte for byte with
 * conformance/golden/*.txt — the contract every ccharts binding is held to.
 *
 * The suite lives at the repository root, outside the published package, so a
 * consumer installing from npm simply skips it.
 */

import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { Chart, Color } from "../src/index.js";

const SUITE_DIR = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..",
  "conformance");

const NAMED_COLORS = {
  black: Color.black, red: Color.red, green: Color.green, yellow: Color.yellow,
  blue: Color.blue, magenta: Color.magenta, cyan: Color.cyan, white: Color.white,
  bright_black: Color.brightBlack, bright_red: Color.brightRed,
  bright_green: Color.brightGreen, bright_yellow: Color.brightYellow,
  bright_blue: Color.brightBlue, bright_magenta: Color.brightMagenta,
  bright_cyan: Color.brightCyan, bright_white: Color.brightWhite,
  reset: Color.reset,
};

function color(name) {
  if (name === null || name === undefined) return undefined;
  const escape = NAMED_COLORS[name];
  assert.ok(escape, `unknown color in cases.json: ${name}`);
  return escape;
}

test("matches the shared goldens", { skip: !existsSync(join(SUITE_DIR, "cases.json")) },
  () => {
    const suite = JSON.parse(readFileSync(join(SUITE_DIR, "cases.json"), "utf8"));
    assert.ok(suite.cases.length >= 10, "conformance suite looks truncated");

    for (const testCase of suite.cases) {
      const settings = testCase.settings;
      let rendered;
      if (testCase.chart === "pie") {
        rendered = Chart.pie(
          testCase.slices.map((slice) => ({ label: slice.label ?? null, value: slice.value })),
          {
            width: testCase.width,
            height: testCase.height,
            donut: settings.donut ?? false,
            colors: settings.colors
              ? settings.colors.map((name) => color(name))
              : undefined,
            showLegend: settings.show_legend ?? true,
            showPct: settings.show_pct ?? false,
          });
      } else {
        const dataset = suite.datasets[testCase.dataset];
        const chart = testCase.source === "json"
          ? Chart.fromJson(dataset.json)
          : Chart.fromArrays(dataset.open, dataset.high, dataset.low, dataset.close,
                             dataset.ts);
        try {
          const options = {
            width: testCase.width,
            height: testCase.height,
            riseColor: color(settings.rise_color),
            fallColor: color(settings.fall_color),
            backgroundColor: color(settings.bg_color),
            areaColor: color(settings.area_color),
            singleColor: settings.single_color,
            showPrices: settings.show_prices,
            showTimes: settings.show_times,
            plain: settings.plain ?? false,
          };
          rendered = testCase.chart === "line"
            ? chart.line(options)
            : chart.candle(options);
        } finally {
          chart.free();
        }
      }
      const expected = readFileSync(join(SUITE_DIR, "golden", `${testCase.name}.txt`),
        "utf8");
      assert.equal(rendered, expected, `${testCase.name} differs from its golden`);
    }
  });
