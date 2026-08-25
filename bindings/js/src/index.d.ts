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
/**
 * Render with no ANSI escapes at all, overriding every color. Use it when
 * the chart is going somewhere that does not interpret escapes — a log
 * file, an HTML block, a commit message. Default false.
 */
  plain?: boolean;
}

/** One slice of a pie chart. */
export interface PieSlice {
  /** Legend label; null/undefined omits it. */
  label?: string | null;
  /**
   * A positive amount; the pie computes the percentage from the sum. A value
   * `<= 0` (zero, negative, NaN, inf) makes the whole render return the empty
   * string rather than an error.
   */
  value: number;
}

/** Options accepted by {@link Chart.pie}. */
export interface PieOptions {
  /** Chart width in cells. Default 24. */
  width?: number;
  /** Chart height in cells. Default 10. */
  height?: number;
  /** Hollow out the center (a donut) instead of a filled disk. Default false. */
  donut?: boolean;
  /**
   * Per-slice ANSI escapes; slice `i` uses `colors[i % colors.length]`.
   * null/undefined entries mean the default palette color for that index.
   * Default: the fixed default palette.
   */
  colors?: Array<string | null>;
  /** Print one `label  value (pct%)` line per slice below the disk. Default true. */
  showLegend?: boolean;
  /** Append `(NN%)` to each legend entry. Default false. */
  showPct?: boolean;
  /** Angular gap between slices, in radians. Default 0 (adjacent). */
  sliceGap?: number;
  /**
   * Donut thickness in `[0, 1]`: `0` = a filled disk, `1` = a hairline ring.
   * Omitted (default), the thickness comes from `donut` (0.5 for a donut,
   * 0 for a disk). Values above 1 are clamped.
   */
  innerRadiusRatio?: number;
  /**
   * Legend entry format: `0` = `label  value` (+ `(NN%)` with `showPct`),
   * `1` = `label  NN%`, `2` = `value  (NN%)`, `3` = `label` only.
   * Unknown values fall back to 0. Default 0.
   */
  legendFormat?: number;
  /**
   * Angle (radians) at which slice 0 begins. Omitted (default) uses the
   * library default (12 o'clock).
   */
  startAngle?: number;
  /** Sweep the slices clockwise instead of the default counter-clockwise. Default false. */
  counterClockwise?: boolean;
  /** Text drawn in the hollow center of a donut (only when there is a hollow). Default: none. */
  centerText?: string;
}

/** Options accepted by {@link Chart.histogram}. */
export interface HistogramOptions {
  /** Chart width in cells. Default 60. */
  width?: number;
  /** Chart height in cells. Default 8. */
  height?: number;
  /**
   * Number of equal-width bins; `0` selects a value from the sample count,
   * bounded by the chart width. Default 0 (auto).
   */
  binCount?: number;
  /**
   * Lower edge of the value window. Omitted/`undefined` (or `NaN`) uses the
   * data minimum.
   */
  minValue?: number;
  /**
   * Upper edge of the value window. Omitted/`undefined` (or `NaN`) uses the
   * data maximum.
   */
  maxValue?: number;
  /** ANSI escape for the bars. Default green. */
  color?: string;
  /** ANSI escape filling empty cells. Default: the terminal background. */
  backgroundColor?: string;
  /** Print a value-axis footer row (window min left, max right). Default false. */
  showBins?: boolean;
  /** Print the max-count / min-count labels in a left margin. Default false. */
  showPrices?: boolean;
  /**
   * Render with no ANSI escapes at all, overriding every color. Default false.
   */
  plain?: boolean;
}

/** Options accepted by {@link Chart.sparkline}. */
export interface SparklineOptions {
  /** Chart width in cells. Default 24. */
  width?: number;
  /** Chart height in cells. Default 1 (a single row). */
  height?: number;
  /** ANSI escape for the trend line. Default green. */
  riseColor?: string;
  /** ANSI escape filling the area under the line. Default: nothing. */
  areaColor?: string;
  /**
   * Reserve this many sub-pixels at the top edge so the line does not clip
   * at the very top of a tiny chart. Default 0.
   */
  minAbove?: number;
  /**
   * Reserve this many sub-pixels at the bottom edge so the line does not clip
   * at the very bottom of a tiny chart. Default 0.
   */
  minBelow?: number;
  /**
   * Render with no ANSI escapes at all, overriding every color. Default false.
   */
  plain?: boolean;
}

/** Options accepted by {@link Chart.bar}. */
export interface BarOptions {
  /** Chart width in cells. Default 60. */
  width?: number;
  /** Chart height in cells. Default 8. */
  height?: number;
  /** ANSI escape for the bars. Default green. */
  riseColor?: string;
  /** ANSI escape filling empty cells. Default: the terminal background. */
  backgroundColor?: string;
  /**
   * Print each column's label in a footer row below the chart. Default false.
   */
  showLabels?: boolean;
  /**
   * Print the max bar value and 0 (the baseline) in a left value-axis margin.
   * Default false.
   */
  showPrices?: boolean;
  /**
   * Render with no ANSI escapes at all, overriding every color. Default false.
   */
  plain?: boolean;
}

/** One series of a stacked bar chart: a name and one value per category. */
export interface StackSeries {
  /** Series name (for documentation; the per-series color comes from the palette). */
  name?: string;
  /** One value per category; every series must share the same length. */
  values: ArrayLike<number>;
}

/** Options accepted by {@link Chart.stackedBar}. */
export interface StackOptions {
  /** Chart width in cells. Default 60. */
  width?: number;
  /** Chart height in cells. Default 8. */
  height?: number;
  /**
   * Per-series ANSI escapes; series `i` uses `colors[i % colors.length]`.
   * null/undefined entries mean the default palette color for that index.
   * Omitted entirely selects the fixed default palette.
   */
  colors?: Array<string | null>;
  /** ANSI escape filling empty cells. Default: the terminal background. */
  backgroundColor?: string;
  /** Category names for the label footer, one per category. Default: none. */
  categoryLabels?: string[];
  /**
   * Print each column's category label in a footer row below the chart.
   * Default false.
   */
  showLabels?: boolean;
  /**
   * Print the tallest stack total and 0 (the baseline) in a left value-axis
   * margin. Default false.
   */
  showPrices?: boolean;
  /**
   * Render with no ANSI escapes at all, overriding every color (each series
   * color becomes an empty escape). Default false.
   */
  plain?: boolean;
}

/** Options accepted by {@link Chart.heatmap}. */
export interface HeatOptions {
  /** Chart width in cells. Default 24. */
  width?: number;
  /** Chart height in cells. Default 10. */
  height?: number;
  /** ANSI escape for the matrix minimum value. Default: the ladder's low end. */
  lowColor?: string;
  /** ANSI escape for the matrix maximum value. Default: the ladder's high end. */
  highColor?: string;
  /** ANSI escape for the ladder's middle entry (a 3-stop ramp). Default: none. */
  midColor?: string;
  /** ANSI escape filling cells the matrix does not cover. Default: the terminal background. */
  backgroundColor?: string;
  /** Row labels printed around the grid when {@link showLabels} is set, one per row. Default: none. */
  rowLabels?: string[];
  /** Column labels printed around the grid when {@link showLabels} is set, one per column. Default: none. */
  colLabels?: string[];
  /**
   * Print the row/column labels around the grid. Default false.
   */
  showLabels?: boolean;
  /**
   * Render with no ANSI escapes at all, overriding every color. Default false.
   */
  plain?: boolean;
}

/** One category of a box plot: a name and that category's samples. */
export interface BoxCategory {
  /** Category name (the core does not print it; kept for the binding API). */
  name?: string | null;
  /** This category's sample values; every category may have its own length. */
  samples: ArrayLike<number>;
}

/** Options accepted by {@link Chart.boxplot}. */
export interface BoxOptions {
  /** Chart width in cells. Default 60. */
  width?: number;
  /** Chart height in cells. Default 8. */
  height?: number;
  /** ANSI escape for the box and median line. Default green. */
  riseColor?: string;
  /** ANSI escape for the whiskers. Default: same as {@link riseColor}. */
  areaColor?: string;
  /** ANSI escape filling the empty cells above/below a box. Default: the terminal background. */
  backgroundColor?: string;
  /** Print the global max/min value labels in a left margin. Default false. */
  showPrices?: boolean;
  /**
   * Render with no ANSI escapes at all, overriding every color. Default false.
   */
  plain?: boolean;
}
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

  /**
   * Renders a pie/donut chart from the given slices. A pie has no OHLC
   * dataset, so this is a static method taking the slices directly.
   * @throws {CchartsError} on empty input, non-finite values or bad dimensions.
   */
  static pie(slices: PieSlice[], options?: PieOptions): string;

  /**
   * Renders a histogram of the given scalar samples. A histogram has no OHLC
   * dataset, so this is a static method taking the raw sample values.
   * @throws {CchartsError} on empty input, non-finite samples or bad dimensions.
   */
  static histogram(samples: ArrayLike<number>, options?: HistogramOptions): string;

  /**
   * Renders a sparkline of the given scalar samples. A sparkline has no OHLC
   * dataset, so this is a static method taking the raw sample values.
   * @throws {CchartsError} on empty input, non-finite samples or bad dimensions.
   */
  static sparkline(samples: ArrayLike<number>, options?: SparklineOptions): string;

  /**
   * Renders a categorical bar chart of the `(label, value)` pairs formed by
   * the parallel `labels` and `values` arrays. A bar chart has no OHLC
   * dataset, so this is a static method taking the labels and values
   * directly.
   * @throws {CchartsError} on empty or mismatched input, non-finite values
   * or bad dimensions.
   */
  static bar(
    labels: string[],
    values: ArrayLike<number>,
    options?: BarOptions,
  ): string;

  /**
   * Renders a stacked bar chart of the given series. Each series contributes
   * one vertical segment per category, and a category's bar height is the SUM
   * of its series' values. A stacked bar chart has no OHLC dataset, so this
   * is a static method taking the series directly.
   * @throws {CchartsError} on empty or unequal-length input, non-finite
   * values or bad dimensions.
   */
  static stackedBar(series: StackSeries[], options?: StackOptions): string;

  /**
   * Renders a heatmap of a rows x cols row-major values matrix into a
   * width x height grid. Every row must share the same length. A heatmap has
   * no OHLC dataset, so this is a static method taking the matrix directly.
   * @throws {CchartsError} on empty or ragged input, non-finite values or bad
   * dimensions.
   */
  static heatmap(
    values: Array<ArrayLike<number>>,
    options?: HeatOptions,
  ): string;

  /** Releases the dataset. Safe to call more than once. */
  free(): void;
}
