/**
 * Terminal charts for financial OHLC data — the C library ccharts compiled to
 * WebAssembly. The module instantiates itself at import time (top-level
 * await), so there is nothing to initialize.
 */

/** Options accepted by {@link Chart.line} and {@link Chart.candle}. */
export interface ChartOptions {
  /** Chart width in cells. Default 60. */
  width?: number;
  /** Chart height in cells. Default 8. */
  height?: number;
  /** ANSI escape for rising values and candles. Default green. */
  riseColor?: string;
  /** ANSI escape for falling values and candles. Default red. */
  fallColor?: string;
  /** ANSI escape filling empty cells. Default: the terminal background. */
  backgroundColor?: string;
  /** ANSI escape filling the area below a line chart. Default: nothing. */
  areaColor?: string;
  /**
   * Draw the whole chart in one color chosen from the overall change instead
   * of coloring each segment by its own direction. Default false.
   */
  singleColor?: boolean;
  /** Print max/min price labels in a left margin. Default false. */
  showPrices?: boolean;
  /** Print the first and last timestamp under the chart. Default false. */
  showTimes?: boolean;
}

/** An error reported by the chart library. */
export class CchartsError extends Error {
  /** The ccharts_status code: 1 invalid argument, 2 parse, 3 out of memory, 4 non-finite, 5 dimensions. */
  readonly code: number;
  constructor(code: number, message: string);
}

/** The sixteen ANSI colors and the reset sequence, as escape strings. */
export const Color: Readonly<{
  black: string; red: string; green: string; yellow: string;
  blue: string; magenta: string; cyan: string; white: string;
  brightBlack: string; brightRed: string; brightGreen: string;
  brightYellow: string; brightBlue: string; brightMagenta: string;
  brightCyan: string; brightWhite: string; reset: string;
}>;

/** Version of the underlying C library. */
export const version: string;
/** Largest width or height in cells (CC_MAX_DIM). */
export const maxDim: number;
/** Largest number of cells in a chart (CC_MAX_CELLS). */
export const maxCells: number;

/** A parsed OHLC dataset that can be rendered repeatedly. */
export class Chart {
  /**
   * Builds a chart from four equal-length price columns and optional epoch
   * seconds.
   * @throws {CchartsError} on empty, mismatched or non-finite input.
   */
  static fromArrays(
    open: ArrayLike<number>,
    high: ArrayLike<number>,
    low: ArrayLike<number>,
    close: ArrayLike<number>,
    ts?: ArrayLike<number | bigint>,
  ): Chart;

  /**
   * Builds a chart from the fixed-schema JSON document: an array of objects
   * with `ts`, `open`, `high`, `low` and `close`.
   * @throws {CchartsError} when the document cannot be parsed.
   */
  static fromJson(json: string): Chart;

  /**
   * Builds a chart from CSV rows of `open,high,low,close[,timestamp]`.
   * Blank lines are skipped.
   */
  static fromCsv(text: string, valueSeparator?: string, lineSeparator?: string): Chart;

  /** Number of candles in the dataset. */
  readonly length: number;

  /** Renders a line chart of the closing prices. */
  line(options?: ChartOptions): string;

  /** Renders a candlestick chart. */
  candle(options?: ChartOptions): string;

  /** Releases the dataset. Safe to call more than once. */
  free(): void;
}
