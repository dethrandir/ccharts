/**
 * ccharts — financial OHLC data as a string.
 *
 * Line and candlestick charts drawn with Unicode block characters, with ANSI
 * color optional; nothing is printed for you. This is the C library
 * https://github.com/dethrandir/ccharts compiled to WebAssembly, so the same
 * package runs in Node, Deno, Bun and the browser with no native build step
 * and no platform-specific binaries.
 *
 *     import { Chart, Color } from "@dethrandir/ccharts";
 *
 *     const chart = Chart.fromArrays(open, high, low, close, epochSeconds);
 *     console.log(chart.line({ width: 60, height: 8, showPrices: true }));
 *     chart.free();
 *
 * The module instantiates the WebAssembly module at import time (top-level
 * await), so nothing has to be initialized by hand.
 */

import { wasmBase64 } from "./wasm-binary.js";

/** Status codes from the C ABI (ccharts_status). */
const STATUS = {
  OK: 0,
  INVALID_ARGUMENT: 1,
  PARSE: 2,
  OUT_OF_MEMORY: 3,
  NON_FINITE: 4,
  DIMENSIONS: 5,
};

/** An error reported by the chart library. */
export class CchartsError extends Error {
  /**
   * @param {number} code one of the ccharts_status values
   * @param {string} message
   */
  constructor(code, message) {
    super(message);
    this.name = "CchartsError";
    this.code = code;
  }
}

function decodeBase64(base64) {
  if (typeof Buffer !== "undefined") {
    return new Uint8Array(Buffer.from(base64, "base64"));
  }
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

/**
 * The module is built with emscripten's standalone target, which pulls in a
 * handful of WASI stdio calls through snprintf/fprintf. ccharts performs no
 * real I/O — the only fprintf is on an allocation-failure path — so these
 * stubs exist to satisfy instantiation. fd_write still forwards to the
 * console so a diagnostic is never swallowed.
 */
function wasiStubs(getMemory) {
  const decoder = new TextDecoder();
  return {
    fd_write(fd, iovs, iovsLen, nwrittenPtr) {
      const memory = getMemory();
      const view = new DataView(memory.buffer);
      const bytes = new Uint8Array(memory.buffer);
      let written = 0;
      let text = "";
      for (let i = 0; i < iovsLen; i++) {
        const ptr = view.getUint32(iovs + i * 8, true);
        const len = view.getUint32(iovs + i * 8 + 4, true);
        text += decoder.decode(bytes.subarray(ptr, ptr + len));
        written += len;
      }
      view.setUint32(nwrittenPtr, written, true);
      if (text) (fd === 2 ? console.error : console.log)(text.replace(/\n$/, ""));
      return 0;
    },
    fd_close: () => 0,
    fd_seek: () => 0,
    environ_get: () => 0,
    environ_sizes_get(countPtr, sizePtr) {
      const view = new DataView(getMemory().buffer);
      view.setUint32(countPtr, 0, true);
      view.setUint32(sizePtr, 0, true);
      return 0;
    },
    proc_exit: () => {},
  };
}

const wasmModule = await WebAssembly.compile(decodeBase64(wasmBase64));
const instance = await WebAssembly.instantiate(wasmModule, {
  wasi_snapshot_preview1: wasiStubs(() => exports.memory),
  // ALLOW_MEMORY_GROWTH asks to be told when the heap grows so a host can
  // refresh cached views. Nothing here caches one — every helper reads
  // exports.memory.buffer afresh — so the notification is a no-op.
  env: { emscripten_notify_memory_growth: () => {} },
});
const exports = instance.exports;

// WASI reactor modules expose _initialize to run static constructors and set
// up libc. It must be called once before anything else touches the heap.
exports._initialize?.();

const encoder = new TextEncoder();
const decoder = new TextDecoder();

/** Memory can grow, which detaches old views, so never cache them. */
const u8 = () => new Uint8Array(exports.memory.buffer);
const view = () => new DataView(exports.memory.buffer);

function malloc(bytes) {
  const ptr = exports.malloc(bytes);
  if (ptr === 0) throw new CchartsError(STATUS.OUT_OF_MEMORY, "out of memory");
  return ptr;
}

/** Reads a NUL-terminated string from wasm memory. */
function readCString(ptr) {
  if (ptr === 0) return null;
  const bytes = u8();
  let end = ptr;
  while (bytes[end] !== 0) end++;
  return decoder.decode(bytes.subarray(ptr, end));
}

/** Copies a JS string into wasm memory as NUL-terminated UTF-8. */
function writeCString(text) {
  const bytes = encoder.encode(text);
  const ptr = malloc(bytes.length + 1);
  u8().set(bytes, ptr);
  u8()[ptr + bytes.length] = 0;
  return ptr;
}

function fail(status) {
  return new CchartsError(status, readCString(exports.ccharts_error_message(status)));
}

/** The sixteen ANSI colors and the reset sequence, read from the library. */
export const Color = Object.freeze({
  black: readCString(exports.ccharts_color(0)),
  red: readCString(exports.ccharts_color(1)),
  green: readCString(exports.ccharts_color(2)),
  yellow: readCString(exports.ccharts_color(3)),
  blue: readCString(exports.ccharts_color(4)),
  magenta: readCString(exports.ccharts_color(5)),
  cyan: readCString(exports.ccharts_color(6)),
  white: readCString(exports.ccharts_color(7)),
  brightBlack: readCString(exports.ccharts_color(8)),
  brightRed: readCString(exports.ccharts_color(9)),
  brightGreen: readCString(exports.ccharts_color(10)),
  brightYellow: readCString(exports.ccharts_color(11)),
  brightBlue: readCString(exports.ccharts_color(12)),
  brightMagenta: readCString(exports.ccharts_color(13)),
  brightCyan: readCString(exports.ccharts_color(14)),
  brightWhite: readCString(exports.ccharts_color(15)),
  reset: readCString(exports.ccharts_color(16)),
});

/** Version of the underlying C library. */
export const version = readCString(exports.ccharts_version());
/** Largest width or height in cells (CC_MAX_DIM). */
export const maxDim = exports.ccharts_max_dim();
/** Largest number of cells in a chart (CC_MAX_CELLS). */
export const maxCells = exports.ccharts_max_cells();

/**
 * Releases the dataset behind a Chart that was garbage collected without
 * free() being called. Explicit free() is still preferable.
 */
const registry = new FinalizationRegistry((handle) => {
  exports.ccharts_data_free(handle);
});

const SETTINGS_BYTES = 4 * 4 + 3 * 4; // four 32-bit pointers + three int32
const PIE_SLICE_BYTES = 16; // 32-bit label pointer, 4 bytes padding, double value
// ccharts_hist_settings: two 32-bit pointers, int32 bin_count, padding, then
// two doubles (8-byte aligned), then two int32. 4+4+4+4 + (8+8) + 4+4 = 40.
const HIST_SETTINGS_BYTES = 40;
// ccharts_spark_settings: two 32-bit pointers then two int32. 4+4+4+4 = 16.
const SPARK_SETTINGS_BYTES = 16;
// ccharts_bar_settings: two 32-bit pointers then two int32. 4+4+4+4 = 16.
// (The bar items reuse PIE_SLICE_BYTES: a 32-bit label pointer, 4 bytes
// padding, then a double value at offset 8.)
const BAR_SETTINGS_BYTES = 16;
// ccharts_stack_series: two 32-bit pointers. 4+4 = 8.
const STACK_SERIES_BYTES = 8;
// ccharts_stack_settings: three 32-bit pointers then four int32. 4*3+4*4 = 28.
const STACK_SETTINGS_BYTES = 28;
// ccharts_heat_settings: six 32-bit pointers then one int32. 4*6+4 = 28.
const HEAT_SETTINGS_BYTES = 28;
// ccharts_box_category: name pointer, samples pointer, int32 n. 4+4+4 = 12.
const BOX_CATEGORY_BYTES = 12;
// ccharts_box_settings: three 32-bit pointers then one int32. 4*3+4 = 16.
const BOX_SETTINGS_BYTES = 16;
const PTR_BYTES = 4;

/**
 * Allocates and writes an ANSI color string into wasm memory, returning its
 * pointer. An empty C string tells the library to emit no escape at all,
 * which is different from a null pointer (use the default color); `plain`
 * forces the empty string over every color. The pointer is pushed onto
 * `owned` so the caller frees it in one pass.
 */
function colorPtr(owned, plain, color) {
  if (plain) {
    const ptr = writeCString("");
    owned.push(ptr);
    return ptr;
  }
  if (color === undefined || color === null || color === "") return 0;
  const ptr = writeCString(String(color));
  owned.push(ptr);
  return ptr;
}

/** A parsed OHLC dataset that can be rendered repeatedly. */
export class Chart {
  #handle;
  #length;
  #token;

  constructor(handle, length) {
    if (handle === 0) throw new CchartsError(STATUS.OUT_OF_MEMORY, "out of memory");
    this.#handle = handle;
    this.#length = length;
    this.#token = Symbol("ccharts.chart");
    registry.register(this, handle, this.#token);
  }

  /**
   * Builds a chart from four equal-length price columns.
   *
   * @param {ArrayLike<number>} open
   * @param {ArrayLike<number>} high
   * @param {ArrayLike<number>} low
   * @param {ArrayLike<number>} close
   * @param {ArrayLike<number|bigint>} [ts] epoch seconds
   * @returns {Chart}
   */
  static fromArrays(open, high, low, close, ts) {
    const n = open.length;
    if (n === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one candle");
    }
    if (high.length !== n || low.length !== n || close.length !== n) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT,
        "open, high, low and close must have the same length");
    }
    if (ts && ts.length !== n) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT,
        "ts must have the same length as the price columns");
    }

    const columns = [open, high, low, close];
    const pointers = [];
    let tsPtr = 0;
    let handleOut = 0;
    try {
      for (const column of columns) {
        const ptr = malloc(n * 8);
        pointers.push(ptr);
        new Float64Array(exports.memory.buffer, ptr, n).set(
          column instanceof Float64Array ? column : Float64Array.from(column, Number));
      }
      if (ts) {
        tsPtr = malloc(n * 8);
        const epochs = new BigInt64Array(exports.memory.buffer, tsPtr, n);
        for (let i = 0; i < n; i++) epochs[i] = BigInt(ts[i]);
      }
      handleOut = malloc(4);

      const status = exports.ccharts_from_arrays(
        pointers[0], pointers[1], pointers[2], pointers[3], tsPtr, n, handleOut);
      if (status !== STATUS.OK) throw fail(status);
      return new Chart(view().getUint32(handleOut, true), n);
    } finally {
      for (const ptr of pointers) exports.free(ptr);
      if (tsPtr) exports.free(tsPtr);
      if (handleOut) exports.free(handleOut);
    }
  }

  /**
   * Builds a chart from the fixed-schema JSON document: an array of objects
   * with ts, open, high, low and close.
   *
   * @param {string} json
   * @returns {Chart}
   */
  static fromJson(json) {
    return Chart.#parse(json, (ptr, out) => exports.ccharts_parse_json(ptr, out));
  }

  /**
   * Builds a chart from CSV rows of open,high,low,close[,timestamp].
   * Blank lines are skipped.
   *
   * @param {string} text
   * @param {string} [valueSeparator=","]
   * @param {string} [lineSeparator="\n"]
   * @returns {Chart}
   */
  static fromCsv(text, valueSeparator = ",", lineSeparator = "\n") {
    const vsep = valueSeparator.charCodeAt(0);
    const lsep = lineSeparator.charCodeAt(0);
    return Chart.#parse(text,
      (ptr, out) => exports.ccharts_parse_csv(ptr, vsep, lsep, out));
  }

  static #parse(text, call) {
    if (typeof text !== "string") {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "expected a string");
    }
    let textPtr = 0;
    let handleOut = 0;
    try {
      textPtr = writeCString(text);
      handleOut = malloc(4);
      const status = call(textPtr, handleOut);
      if (status !== STATUS.OK) throw fail(status);
      const handle = view().getUint32(handleOut, true);
      return new Chart(handle, exports.ccharts_data_len(handle));
    } finally {
      if (textPtr) exports.free(textPtr);
      if (handleOut) exports.free(handleOut);
    }
  }

  /**
   * Renders a pie or donut chart from the given slices. A pie has no OHLC
   * dataset, so this is a static method taking the slices directly.
   *
   * @param {Array<{label?: string|null, value: number}>} slices
   * @param {PieOptions} [options]
   * @returns {string}
   */
  static pie(slices, options = {}) {
    const {
      width = 24, height = 10,
      donut = false, colors, showLegend = true, showPct = false,
      sliceGap = 0, innerRadiusRatio = -1, legendFormat = 0,
      startAngle = -1, counterClockwise = false, centerText,
    } = options;

    if (!Array.isArray(slices) || slices.length === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one slice");
    }
    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];
    let slicesPtr = 0;
    let colorsPtr = 0;
    let centerTextPtr = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      slicesPtr = malloc(slices.length * PIE_SLICE_BYTES);
      owned.push(slicesPtr);
      const dv = view();
      for (let i = 0; i < slices.length; i++) {
        const slice = slices[i];
        const label = slice.label === undefined || slice.label === null || slice.label === ""
          ? 0 : writeCString(String(slice.label));
        if (label) owned.push(label);
        dv.setUint32(slicesPtr + i * PIE_SLICE_BYTES, label, true);
        dv.setFloat64(slicesPtr + i * PIE_SLICE_BYTES + 8, slice.value, true);
      }

      let colorCount = 0;
      if (colors && colors.length > 0) {
        colorsPtr = malloc(colors.length * PTR_BYTES);
        owned.push(colorsPtr);
        for (let i = 0; i < colors.length; i++) {
          const color = colors[i];
          const ptr = color === undefined || color === null || color === ""
            ? 0 : writeCString(String(color));
          if (ptr) owned.push(ptr);
          dv.setUint32(colorsPtr + i * PTR_BYTES, ptr, true);
        }
        colorCount = colors.length;
      }

      if (centerText !== undefined && centerText !== null && centerText !== "") {
        centerTextPtr = writeCString(String(centerText));
        owned.push(centerTextPtr);
      }

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = exports.ccharts_pie_from_slices(
        slicesPtr, slices.length, width, height,
        donut ? 1 : 0, colorsPtr, colorCount,
        showLegend ? 1 : 0, showPct ? 1 : 0,
        sliceGap, innerRadiusRatio, legendFormat,
        startAngle, counterClockwise ? 1 : 0, centerTextPtr,
        outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /**
   * Renders a histogram of the given scalar samples. A histogram has no OHLC
   * dataset, so this is a static method taking the raw sample values.
   *
   * @param {ArrayLike<number>} samples the scalar values to bin
   * @param {HistogramOptions} [options]
   * @returns {string}
   */
  static histogram(samples, options = {}) {
    const {
      width = 60, height = 8,
      binCount = 0, minValue = Number.NaN, maxValue = Number.NaN,
      color, backgroundColor, showBins = false, showPrices = false,
      plain = false,
    } = options;

    if (!Array.isArray(samples) || samples.length === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one sample");
    }
    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];
    let samplesPtr = 0;
    let settings = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      samplesPtr = malloc(samples.length * 8);
      owned.push(samplesPtr);
      new Float64Array(exports.memory.buffer, samplesPtr, samples.length).set(
        samples instanceof Float64Array ? samples : Float64Array.from(samples, Number));

      settings = malloc(HIST_SETTINGS_BYTES);
      owned.push(settings);
      const rise = colorPtr(owned, plain, color);
      const background = colorPtr(owned, plain, backgroundColor);

      const dv = view();
      dv.setUint32(settings, rise, true);
      dv.setUint32(settings + 4, background, true);
      dv.setInt32(settings + 8, binCount, true);
      // NaN min_value/max_value is the "auto" sentinel in the C library.
      dv.setFloat64(settings + 16, minValue, true);
      dv.setFloat64(settings + 24, maxValue, true);
      dv.setInt32(settings + 32, showBins ? 1 : 0, true);
      dv.setInt32(settings + 36, showPrices ? 1 : 0, true);

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = exports.ccharts_hist(
        samplesPtr, samples.length, width, height, settings, outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /**
   * Renders a sparkline of the given scalar samples. A sparkline has no OHLC
   * dataset, so this is a static method taking the raw sample values.
   *
   * @param {ArrayLike<number>} samples the close-like trend values
   * @param {SparklineOptions} [options]
   * @returns {string}
   */
  static sparkline(samples, options = {}) {
    const {
      width = 24, height = 1,
      riseColor, areaColor, minAbove = 0, minBelow = 0,
      plain = false,
    } = options;

    if (!Array.isArray(samples) || samples.length === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one sample");
    }
    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];
    let samplesPtr = 0;
    let settings = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      samplesPtr = malloc(samples.length * 8);
      owned.push(samplesPtr);
      new Float64Array(exports.memory.buffer, samplesPtr, samples.length).set(
        samples instanceof Float64Array ? samples : Float64Array.from(samples, Number));

      settings = malloc(SPARK_SETTINGS_BYTES);
      owned.push(settings);
      const rise = colorPtr(owned, plain, riseColor);
      const area = colorPtr(owned, plain, areaColor);

      const dv = view();
      dv.setUint32(settings, rise, true);
      dv.setUint32(settings + 4, area, true);
      dv.setInt32(settings + 8, minAbove, true);
      dv.setInt32(settings + 12, minBelow, true);

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = exports.ccharts_spark(
        samplesPtr, samples.length, width, height, settings, outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /**
   * Renders a categorical bar chart of the (label, value) pairs formed by the
   * parallel `labels` and `values` arrays. A bar chart has no OHLC dataset,
   * so this is a static method taking the labels and values directly.
   *
   * @param {string[]} labels the categorical label of each bar
   * @param {ArrayLike<number>} values the height of each bar
   * @param {BarOptions} [options]
   * @returns {string}
   */
  static bar(labels, values, options = {}) {
    const {
      width = 60, height = 8,
      riseColor, backgroundColor, showLabels = false, showPrices = false,
      plain = false,
    } = options;

    if (!Array.isArray(labels) || labels.length === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one bar");
    }
    if (!Array.isArray(values) || values.length !== labels.length) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT,
        "labels and values must have the same length");
    }
    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];
    let itemsPtr = 0;
    let settings = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      itemsPtr = malloc(labels.length * PIE_SLICE_BYTES);
      owned.push(itemsPtr);
      const dv = view();
      for (let i = 0; i < labels.length; i++) {
        const label = writeCString(String(labels[i]));
        owned.push(label);
        dv.setUint32(itemsPtr + i * PIE_SLICE_BYTES, label, true);
        dv.setFloat64(itemsPtr + i * PIE_SLICE_BYTES + 8, values[i], true);
      }

      settings = malloc(BAR_SETTINGS_BYTES);
      owned.push(settings);
      const rise = colorPtr(owned, plain, riseColor);
      const background = colorPtr(owned, plain, backgroundColor);
      dv.setUint32(settings, rise, true);
      dv.setUint32(settings + 4, background, true);
      dv.setInt32(settings + 8, showLabels ? 1 : 0, true);
      dv.setInt32(settings + 12, showPrices ? 1 : 0, true);

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = exports.ccharts_bar(
        itemsPtr, labels.length, width, height, settings, outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /** Number of candles in the dataset. */
  get length() {
    return this.#length;
  }

  /**
   * Renders a stacked bar chart of the given series. Each series contributes
   * one vertical segment per category, and a category's bar height is the SUM
   * of its series' values. A stacked bar chart has no OHLC dataset, so this
   * is a static method taking the series directly.
   *
   * @param {Array<{name?: string, values: ArrayLike<number>}>} series each
   *   series must carry the same number of values (one per category)
   * @param {StackOptions} [options]
   * @returns {string}
   */
  static stackedBar(series, options = {}) {
    const {
      width = 60, height = 8,
      colors, backgroundColor, categoryLabels,
      showLabels = false, showPrices = false, plain = false,
    } = options;

    if (!Array.isArray(series) || series.length === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one series");
    }
    const cats = series[0].values.length;
    for (const s of series) {
      if (!s.values || s.values.length !== cats) {
        throw new CchartsError(STATUS.INVALID_ARGUMENT,
          "all series must have the same number of values");
      }
    }
    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];
    let seriesPtr = 0;
    let colorsPtr = 0;
    let catLabelsPtr = 0;
    let settings = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      seriesPtr = malloc(series.length * STACK_SERIES_BYTES);
      owned.push(seriesPtr);
      const dv = view();
      for (let i = 0; i < series.length; i++) {
        const name = writeCString(String(series[i].name ?? ""));
        owned.push(name);
        dv.setUint32(seriesPtr + i * STACK_SERIES_BYTES, name, true);

        const values = series[i].values;
        const vp = malloc(values.length * 8);
        owned.push(vp);
        new Float64Array(exports.memory.buffer, vp, values.length).set(
          values instanceof Float64Array ? values : Float64Array.from(values, Number));
        dv.setUint32(seriesPtr + i * STACK_SERIES_BYTES + 4, vp, true);
      }

      // colors is a NULL-terminated per-series palette override. `plain`
      // forces every entry to the empty C string (emit no escape); otherwise
      // a null/undefined entry means "use the default palette color", and a
      // null array selects the fixed default palette entirely.
      const colorCount = plain ? series.length : (colors ? colors.length : 0);
      if (colorCount > 0) {
        colorsPtr = malloc((colorCount + 1) * PTR_BYTES);
        owned.push(colorsPtr);
        for (let i = 0; i < colorCount; i++) {
          const ptr = plain ? writeCString("")
            : (colors[i] === undefined || colors[i] === null || colors[i] === ""
              ? 0 : writeCString(String(colors[i])));
          if (ptr) owned.push(ptr);
          dv.setUint32(colorsPtr + i * PTR_BYTES, ptr, true);
        }
        dv.setUint32(colorsPtr + colorCount * PTR_BYTES, 0, true); // terminator
      }

      if (categoryLabels && categoryLabels.length > 0) {
        catLabelsPtr = malloc((categoryLabels.length + 1) * PTR_BYTES);
        owned.push(catLabelsPtr);
        for (let i = 0; i < categoryLabels.length; i++) {
          const ptr = writeCString(String(categoryLabels[i]));
          owned.push(ptr);
          dv.setUint32(catLabelsPtr + i * PTR_BYTES, ptr, true);
        }
        dv.setUint32(catLabelsPtr + categoryLabels.length * PTR_BYTES, 0, true);
      }

      const background = colorPtr(owned, plain, backgroundColor);

      settings = malloc(STACK_SETTINGS_BYTES);
      owned.push(settings);
      dv.setUint32(settings, colorsPtr, true);
      dv.setUint32(settings + 4, background, true);
      dv.setUint32(settings + 8, catLabelsPtr, true);
      dv.setInt32(settings + 12, series.length, true);
      dv.setInt32(settings + 16, cats, true);
      dv.setInt32(settings + 20, showLabels ? 1 : 0, true);
      dv.setInt32(settings + 24, showPrices ? 1 : 0, true);

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = exports.ccharts_stack(
        seriesPtr, series.length, width, height, settings, outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /**
   * Renders a heatmap of a rows x cols row-major values matrix into a
   * width x height grid. Every row must share the same length.
   *
   * Matrix elements map to the fixed deterministic colormap ladder by their
   * position between the matrix min/max; a matrix larger than the grid is
   * downsampled by block-average, and one smaller than the grid occupies the
   * top-left with background padding. A heatmap has no OHLC dataset, so this
   * is a static method taking the matrix directly.
   *
   * @param {Array<ArrayLike<number>>} values the matrix, one {cols}-long row
   *   per {rows} sub-array
   * @param {HeatOptions} [options]
   * @returns {string}
   */
  static heatmap(values, options = {}) {
    const {
      width = 24, height = 10,
      lowColor, highColor, midColor, backgroundColor,
      rowLabels, colLabels, showLabels = false, plain = false,
    } = options;

    if (!Array.isArray(values) || values.length === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one matrix row");
    }
    const rows = values.length;
    const cols = values[0].length;
    if (cols === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT,
        "matrix columns must not be empty");
    }
    for (const row of values) {
      if (!row || row.length !== cols) {
        throw new CchartsError(STATUS.INVALID_ARGUMENT,
          "all rows must have the same number of values");
      }
    }
    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];
    let valuesPtr = 0;
    let settings = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      valuesPtr = malloc(rows * cols * 8);
      owned.push(valuesPtr);
      const flat = new Float64Array(exports.memory.buffer, valuesPtr, rows * cols);
      let i = 0;
      for (const row of values) {
        for (const value of row) flat[i++] = Number(value);
      }

      const dv = view();
      // row_labels / col_labels are `rows` / `cols` entry C-string pointer
      // arrays (indexed by the renderer; a NULL-terminator is defensive).
      // They are plain text, so `plain` does not affect them.
      const labelArray = (labels) => {
        if (!labels || labels.length === 0) return 0;
        const arr = malloc((labels.length + 1) * PTR_BYTES);
        owned.push(arr);
        for (let j = 0; j < labels.length; j++) {
          const ptr = writeCString(String(labels[j]));
          owned.push(ptr);
          dv.setUint32(arr + j * PTR_BYTES, ptr, true);
        }
        dv.setUint32(arr + labels.length * PTR_BYTES, 0, true);
        return arr;
      };

      settings = malloc(HEAT_SETTINGS_BYTES);
      owned.push(settings);
      dv.setUint32(settings, colorPtr(owned, plain, lowColor), true);
      dv.setUint32(settings + 4, colorPtr(owned, plain, highColor), true);
      dv.setUint32(settings + 8, colorPtr(owned, plain, midColor), true);
      dv.setUint32(settings + 12, colorPtr(owned, plain, backgroundColor), true);
      dv.setUint32(settings + 16, labelArray(rowLabels), true);
      dv.setUint32(settings + 20, labelArray(colLabels), true);
      dv.setInt32(settings + 24, showLabels ? 1 : 0, true);

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = exports.ccharts_heat(
        valuesPtr, rows, cols, width, height, settings, outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /**
   * Renders a box plot of per-category samples into a width x height grid.
   * Each category carries its own (possibly ragged) samples array; the C core
   * computes a nearest-rank five-number summary per category and draws each
   * box and its whiskers over the global min/max span, so this passes raw
   * samples and settings only. A box plot has no OHLC dataset, so this is a
   * static method taking the categories directly.
   *
   * @param {Array<{name?: string|null, samples: ArrayLike<number>}>} series
   * @param {BoxOptions} [options]
   * @returns {string}
   */
  static boxplot(series, options = {}) {
    const {
      width = 60, height = 8,
      riseColor, areaColor, backgroundColor,
      showPrices = false, plain = false,
    } = options;

    if (!Array.isArray(series) || series.length === 0) {
      throw new CchartsError(STATUS.INVALID_ARGUMENT, "need at least one category");
    }
    for (const cat of series) {
      if (!cat.samples || cat.samples.length === 0) {
        throw new CchartsError(STATUS.INVALID_ARGUMENT,
          "every category must have at least one sample");
      }
    }
    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];
    let catsPtr = 0;
    let settings = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      catsPtr = malloc(series.length * BOX_CATEGORY_BYTES);
      owned.push(catsPtr);
      const dv = view();
      for (let i = 0; i < series.length; i++) {
        const empty = series[i].name === undefined || series[i].name === null;
        const name = empty ? 0 : writeCString(String(series[i].name));
        if (name) owned.push(name);
        dv.setUint32(catsPtr + i * BOX_CATEGORY_BYTES, name, true);

        const samples = series[i].samples;
        const sp = malloc(samples.length * 8);
        owned.push(sp);
        new Float64Array(exports.memory.buffer, sp, samples.length).set(
          samples instanceof Float64Array ? samples : Float64Array.from(samples, Number));
        dv.setUint32(catsPtr + i * BOX_CATEGORY_BYTES + 4, sp, true);
        dv.setInt32(catsPtr + i * BOX_CATEGORY_BYTES + 8, samples.length, true);
      }

      settings = malloc(BOX_SETTINGS_BYTES);
      owned.push(settings);
      dv.setUint32(settings, colorPtr(owned, plain, riseColor), true);
      dv.setUint32(settings + 4, colorPtr(owned, plain, areaColor), true);
      dv.setUint32(settings + 8, colorPtr(owned, plain, backgroundColor), true);
      dv.setInt32(settings + 12, showPrices ? 1 : 0, true);

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = exports.ccharts_box(
        catsPtr, series.length, width, height, settings, outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /**
   * Renders a line chart of the closing prices.
   * @param {ChartOptions} [options]
   * @returns {string}
   */
  line(options) {
    return this.#render(exports.ccharts_line, options);
  }

  /**
   * Renders a candlestick chart.
   * @param {ChartOptions} [options]
   * @returns {string}
   */
  candle(options) {
    return this.#render(exports.ccharts_candle, options);
  }

  #render(draw, options = {}) {
    if (this.#handle === 0) throw new CchartsError(STATUS.INVALID_ARGUMENT,
      "chart has been freed");

    const {
      width = 60, height = 8,
      riseColor, fallColor, backgroundColor, areaColor,
      singleColor = false, showPrices = false, showTimes = false,
      plain = false,
    } = options;

    if (!Number.isInteger(width) || !Number.isInteger(height)) {
      throw new CchartsError(STATUS.DIMENSIONS, "width and height must be integers");
    }

    const owned = [];

    let settings = 0;
    let outPtr = 0;
    let lenPtr = 0;
    try {
      settings = malloc(SETTINGS_BYTES);
      owned.push(settings);
      const rise = colorPtr(owned, plain, riseColor);
      const fall = colorPtr(owned, plain, fallColor);
      const background = colorPtr(owned, plain, backgroundColor);
      const area = colorPtr(owned, plain, areaColor);

      const dv = view();
      dv.setUint32(settings, rise, true);
      dv.setUint32(settings + 4, fall, true);
      dv.setUint32(settings + 8, background, true);
      dv.setUint32(settings + 12, area, true);
      dv.setInt32(settings + 16, singleColor ? 1 : 0, true);
      dv.setInt32(settings + 20, showPrices ? 1 : 0, true);
      dv.setInt32(settings + 24, showTimes ? 1 : 0, true);

      outPtr = malloc(4);
      lenPtr = malloc(4);
      const status = draw(this.#handle, width, height, settings, outPtr, lenPtr);
      if (status !== STATUS.OK) throw fail(status);

      const dv2 = view();
      const chartPtr = dv2.getUint32(outPtr, true);
      const length = dv2.getUint32(lenPtr, true);
      try {
        return decoder.decode(u8().subarray(chartPtr, chartPtr + length));
      } finally {
        exports.ccharts_string_free(chartPtr);
      }
    } finally {
      for (const ptr of owned) exports.free(ptr);
      if (outPtr) exports.free(outPtr);
      if (lenPtr) exports.free(lenPtr);
    }
  }

  /** Releases the dataset. Safe to call more than once. */
  free() {
    if (this.#handle !== 0) {
      registry.unregister(this.#token);
      exports.ccharts_data_free(this.#handle);
      this.#handle = 0;
    }
  }
}
