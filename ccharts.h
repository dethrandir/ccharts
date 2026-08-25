/*
 * ccharts.h — single-header OHLC charts, rendered to a string.
 *
 * Turns financial OHLC data into text: line and candlestick charts drawn with
 * Unicode block characters. Nothing is printed and no stream is written to —
 * the chart functions return a malloc'd string, so the result can go to a
 * terminal, a log, an HTML block or a file just as easily.
 *
 * Color is optional. A color left NULL takes the library default (green
 * rising, red falling); a color set to the EMPTY STRING makes that element
 * carry no escape sequence at all, so setting all four yields plain text.
 *
 * ---------------------------------------------------------------------------
 * USAGE
 * ---------------------------------------------------------------------------
 * Define CCHARTS_IMPLEMENTATION in exactly ONE translation unit before the
 * include; every other TU includes the header as-is (declarations only):
 *
 *     #define CCHARTS_IMPLEMENTATION
 *     #include "ccharts.h"
 *
 * Minimal example:
 *
 *     const char* json =
 *         "[{\"open\":1,\"high\":2,\"low\":0.5,\"close\":1.5}]";
 *     cc_ohlc_t* ohlc = NULL;
 *     int size = 0;
 *     cc_json_to_ohlc(json, &ohlc, &size);
 *
 *     cc_settings_t s = { .rise_color = CC_COLOR_BLUE };
 *     char* chart = cc_line_create(ohlc, size, 60, 8, &s);
 *     printf("%s\n", chart);
 *     free(chart);
 *     free(ohlc);
 *
 * ---------------------------------------------------------------------------
 * DATA FLOW
 * ---------------------------------------------------------------------------
 *   1. Parse raw data into a heap-allocated cc_ohlc_t array:
 *        - cc_str_to_ohlc()  CSV text (open,high,low,close[,timestamp])
 *        - cc_json_to_ohlc() fixed-schema JSON (see its doc comment)
 *   2. Optionally override rendering via a cc_settings_t. Unspecified (NULL /
 *      0) fields fall back to defaults; pass NULL to use all defaults.
 *   3. Call cc_line_create() / cc_candle_create() to get a malloc'd string.
 *   4. Print the string, then free it AND the cc_ohlc_t array.
 *
 * ---------------------------------------------------------------------------
 * RENDERING MODEL
 * ---------------------------------------------------------------------------
 *   Line charts   : each cell is 8 pixels tall via the 1/8 blocks ▁▂▃▄▅▆▇█,
 *                   so curves are smooth. Width keeps the same semantics.
 *   Candle charts : cells are 2 pixels tall (▀/▄/█). A candle draws a solid
 *                   body between open/close and a thin vertical wick (│)
 *                   between high/low; when there is room (width >= size) a
 *                   gap column separates neighboring candles.
 *   Both charts   : optionally prepend a left price margin (min/max) and
 *                   append a time footer (first/last timestamp). The time
 *                   format is chosen automatically from the average interval.
 *
 * MEMORY OWNERSHIP
 *   cc_str_to_ohlc / cc_json_to_ohlc allocate the cc_ohlc_t array (free it).
 *   cc_line_create / cc_candle_create return a malloc'd string (free it).
 *
 * INPUT CONSTRAINTS
 *   - size (number of candles) must be positive for the renderers and the
 *     CSV parser; the JSON parser requires at least one object.
 *   - width/height are clamped to CC_MAX_DIM cells per side and
 *     CC_MAX_CELLS total; invalid dimensions make the chart functions fail
 *     cleanly (empty string) instead of attempting a giant allocation.
 *   - CSV lines may be arbitrarily long (each line is heap-copied), and at
 *     most `size` lines are read.
 *   - JSON must match the fixed schema (see cc_json_to_ohlc); the scanner is
 *     an iterative mini-parser, so stray substrings in string values cannot
 *     cause false key matches.
 *   - ISO8601 timestamps may carry a UTC offset: "+HH:MM", "+HHMM", "+HH"
 *     or "Z" (a missing offset is treated as UTC). The offset is applied, so
 *     the stored epoch is the true instant, not the local wall time.
 *
 * PORTABILITY
 *   Plain C89-compatible code (no VLAs, no C99-only features), so it builds
 *   with GCC, Clang and MSVC out of the box; on Windows gmtime_s replaces
 *   gmtime_r and functions use internal (static) linkage via CC_INLINE.
 */

#ifndef CCHARTS_H
#define CCHARTS_H

/* ============================ ANSI color codes ============================ */

#define CC_COLOR_RESET "\x1b[0m"
#define CC_COLOR_BLACK "\x1b[30m"
#define CC_COLOR_RED "\x1b[31m"
#define CC_COLOR_GREEN "\x1b[32m"
#define CC_COLOR_YELLOW "\x1b[33m"
#define CC_COLOR_BLUE "\x1b[34m"
#define CC_COLOR_MAGENTA "\x1b[35m"
#define CC_COLOR_CYAN "\x1b[36m"
#define CC_COLOR_WHITE "\x1b[37m"
#define CC_COLOR_BRIGHT_BLACK "\x1b[90m"
#define CC_COLOR_BRIGHT_RED "\x1b[91m"
#define CC_COLOR_BRIGHT_GREEN "\x1b[92m"
#define CC_COLOR_BRIGHT_YELLOW "\x1b[93m"
#define CC_COLOR_BRIGHT_BLUE "\x1b[94m"
#define CC_COLOR_BRIGHT_MAGENTA "\x1b[95m"
#define CC_COLOR_BRIGHT_CYAN "\x1b[96m"
#define CC_COLOR_BRIGHT_WHITE "\x1b[97m"

/* ============================ Block element characters ============================
 * The 1/8..7/8 left-growing bars (▏▎▍▌▋▊▉) and the 1/8 lower bars (▁▂▃▅▆▇)
 * give sub-cell horizontal/vertical resolution. ▀/▄/█ and the thin vertical
 * line │ (used for candle wicks) round out the drawing primitives.
 * ============================================================================ */

#define CC_BLOCK_FULL "\u2588"
#define CC_BLOCK_7_8 "\u2589"
#define CC_BLOCK_3_4 "\u258A"
#define CC_BLOCK_5_8 "\u258B"
#define CC_BLOCK_1_2 "\u258C"
#define CC_BLOCK_3_8 "\u258D"
#define CC_BLOCK_1_4 "\u258E"
#define CC_BLOCK_1_8 "\u258F"
#define CC_BLOCK_UPPER_HALF "\u2580"
#define CC_BLOCK_LOWER_HALF "\u2584"
#define CC_BLOCK_LOWER_1_8 "\u2581"
#define CC_BLOCK_LOWER_2_8 "\u2582"
#define CC_BLOCK_LOWER_3_8 "\u2583"
#define CC_BLOCK_LOWER_5_8 "\u2585"
#define CC_BLOCK_LOWER_6_8 "\u2586"
#define CC_BLOCK_LOWER_7_8 "\u2587"
#define CC_LINE_VERTICAL "\u2502"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================ Portability macros ============================
 * CC_INLINE: MSVC's C mode has no C11 inline-linkage semantics, so on
 * Windows all functions get internal linkage (static). Everywhere else they
 * are static inline, which keeps each translation unit self-contained and
 * avoids any duplicate-symbol or emission problems on any compiler.
 * CC_GMTIME_R: POSIX gmtime_r vs MSVC gmtime_s (note the argument order). */
#if defined(_MSC_VER)
#define CC_INLINE static
#else
#define CC_INLINE static inline
#endif

#ifdef _WIN32
#define CC_GMTIME_R(t, tm) (gmtime_s((tm), (t)) == 0)
#else
#define CC_GMTIME_R(t, tm) (gmtime_r((t), (tm)) != NULL)
#endif

#if defined(NAN)
#define CC_NAN ((double)NAN)
#elif defined(_MSC_VER)
#define CC_NAN ((double)sqrt(-1.0))
#else
#define CC_NAN (0.0 / 0.0)
#endif

/* Chart dimensions are bounded to keep allocations sane and to avoid
 * integer-overflow surprises. A terminal chart needs nowhere near these
 * sizes; dimension checks fail cleanly (empty string / NULL) instead of
 * attempting a giant allocation. */
#define CC_MAX_DIM   100000     /* max width or height in cells */
#define CC_MAX_CELLS 1000000    /* max width * height (total cells) */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================ Public API ============================ */

typedef struct cc_ohlc cc_ohlc_t;

/* Rendering options. Unspecified fields fall back to defaults:
 *   - rise_color  : color for rising values / candles  (default green)
 *   - fall_color  : color for falling values / candles (default red)
 *   - bg_color    : background of empty cells (default: none)
 *   - area_color  : line chart fill below the line (default: none)
 *   - single_color: 1 = whole line in one color chosen from overall change,
 *                   0 = color each segment by its own direction (default)
 *   - show_prices : print max/min price labels in a left margin
 *   - show_times  : print first/last timestamp footer under the chart
 * Build with a designated initializer, e.g.
 *   cc_settings_t s = { .rise_color = CC_COLOR_BLUE };
 * and pass NULL to any chart function for full defaults. */
typedef struct cc_settings {
    const char* rise_color;
    const char* fall_color;
    const char* bg_color;
    const char* area_color;
    int single_color;
    int show_prices;
    int show_times;
} cc_settings_t;

/* Fills missing fields of `settings` with defaults and returns the result.
 * Never NULL — passing NULL yields a struct with all defaults. Used
 * internally by every chart function; safe to call directly. */
CC_INLINE cc_settings_t cc_settings_resolve(const cc_settings_t* settings);

/* Parses CSV text into a heap-allocated cc_ohlc_t array.
 *
 * Format: one candle per line, fields separated by `val_seperator`:
 *     open,high,low,close          (timestamp = 0)
 *     open,high,low,close,timestamp (ISO8601 or epoch seconds)
 *
 * `size` is the expected number of lines; at most `size` lines are read
 * (extra lines are ignored, not overflowed). Lines may be arbitrarily long.
 * On success returns 0 and stores a calloc'd array in *ohlc (caller frees
 * it). Returns non-zero on invalid arguments or allocation failure. */
CC_INLINE int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc,
                             char val_seperator, char line_seperator);

/* Parses a fixed-schema JSON document into a heap-allocated cc_ohlc_t array.
 *
 * Expected schema — an array of objects with string timestamps:
 *     [{"ts":"2026-07-20T00:00:00+00:00","open":328.75,"high":330.0,
 *       "low":323.75,"close":328.0,"volume":46622936}, ...]
 * The "volume" field is ignored; unknown keys are skipped by value. Keys are
 * matched exactly, so a substring inside a string value can never be
 * mistaken for a key. On success returns 0 and stores the number of candles
 * in *size and a calloc'd array in *ohlc (caller frees both). Returns
 * non-zero on malformed input or allocation failure. */
CC_INLINE int cc_json_to_ohlc(const char* json, cc_ohlc_t** ohlc, int* size);

/* Renders a smooth line chart of the close prices.
 * `width`/`height` are the chart plot area in cells, bounded by
 * CC_MAX_DIM / CC_MAX_CELLS. Returns a malloc'd string (caller frees) that
 * may additionally contain a left price margin and a time footer depending
 * on the settings flags; invalid dimensions yield an empty string. */
CC_INLINE char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height,
                               const cc_settings_t* settings);

/* Renders a candlestick chart from the OHLC data.
 * `width`/`height` are the chart plot area in cells, bounded by
 * CC_MAX_DIM / CC_MAX_CELLS. When width >= size each candle is a few cells
 * wide with a gap between neighbors; when width < size neighboring candles
 * are aggregated into virtual candles (like the line chart's downsampling).
 * Returns a malloc'd string (caller frees); invalid dimensions yield an
 * empty string. */
CC_INLINE char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height,
                                 const cc_settings_t* settings);

/* ============================ Pie charts ============================
 * A pie / donut chart turns (label, value) slices into a disk drawn with
 * block characters. Unlike line/candle there is no OHLC data: each slice is
 * an amount, the pie computes the percentages, and slices are drawn from
 * 12 o'clock going counter-clockwise. Colors come from a fixed deterministic
 * palette (never random, so every render is byte-identical — which is what
 * keeps the conformance goldens stable) unless the settings override it. */

/* One pie slice: a label (may be NULL or empty) and a positive value. A slice
 * whose value is not > 0 (zero, negative, or NaN) makes the whole render fail
 * cleanly with an empty string; +inf is rejected upstream (the wrapper and
 * ABI), which keeps this header free of C99-only `isfinite`. */
typedef struct cc_pie_slice {
    const char* label;
    double       value;
} cc_pie_slice_t;

/* Pie/donut rendering options. Unspecified fields fall back to defaults:
 *   - bg_color   : background of cells outside the disk (default: none)
 *   - colors     : per-slice palette override — a NULL-terminated array of
 *                  ANSI color strings — or NULL for the fixed default palette
 *   - donut      : 1 = hollow center (donut), 0 = filled disk (default).
 *                  Only consulted when inner_radius_ratio is left unspecified.
 *   - show_legend: 1 = print one legend line per slice below the disk
 *   - show_pct   : 1 = append "(NN%)" to the default legend entries; ignored
 *                  by the legend_format variants that always show the percent
 *   - slice_gap  : angular gap between slices, in radians. 0 = slices touch
 *                  (default); >0 leaves a thin blank gap between neighbors
 *   - inner_radius_ratio : donut thickness as a fraction of the outer radius,
 *                  in [0,1]. 0 = filled disk; (0,1] = hollow center of that
 *                  radius. NEGATIVE (e.g. -1) = unspecified: the `donut` flag
 *                  then decides — 0.5 for a donut, 0 for a disk. Values > 1
 *                  are clamped to 1.
 *   - legend_format : CC_PIE_LEGEND_* enum controlling each legend entry's
 *                  composition (see below); 0 reproduces the original output
 *   - start_angle : angle (radians) where slice 0 begins. NEGATIVE =
 *                  unspecified (default CC_PI/2, i.e. 12 o'clock)
 *   - counter_clockwise : 0 = library default direction (counter-clockwise,
 *                  matching the original output); nonzero = mirrored
 *                  (clockwise) sweep
 *   - center_text : text drawn in the hollow center. Only shown when there is
 *                  a real hollow (inner_radius_ratio > 0); NULL or "" disables
 *                  it. Truncated to fit the hole.
 * Build with a designated initializer and pass NULL for full defaults. To
 * keep the library defaults for a partial brace initializer, leave the double
 * fields negative (their zero values are meaningful: inner_radius_ratio=0 is
 * a filled disk, start_angle=0 is 3 o'clock) and set inner_radius_ratio =
 * -1.0 when you want the `donut` flag's classic hollow. */
typedef struct cc_pie_settings {
    const char* bg_color;
    const char* const* colors;
    int donut;
    int show_legend;
    int show_pct;
    /* --- Fas 3 pie/donut settings (all optional) --- */
    double slice_gap;            /* radians; 0 = adjacent slices */
    double inner_radius_ratio;   /* [0,1]; <0 = donut flag decides */
    int    legend_format;        /* CC_PIE_LEGEND_* */
    double start_angle;          /* radians; <0 = CC_PI/2 */
    int    counter_clockwise;    /* 0 = default CCW sweep; nonzero = CW */
    const char* center_text;     /* NULL/"" = none; hollow-center label */
} cc_pie_settings_t;

/* legend_format values. All other values fall back to CC_PIE_LEGEND_VALUE
 * (the original "label  value (+ (NN%) when show_pct)" behavior). */
#define CC_PIE_LEGEND_VALUE      0 /* "label  value" (+ " (NN%)" when show_pct) */
#define CC_PIE_LEGEND_LABEL_PCT  1 /* "label  NN%"                          */
#define CC_PIE_LEGEND_VALUE_PCT  2 /* "value  (NN%)"                        */
#define CC_PIE_LEGEND_LABEL      3 /* "label" only                          */

/* Renders a pie/donut chart of `count` slices into a `width` x `height` grid
 * (cells) and returns a malloc'd string (caller frees). Slices are normalized
 * to angles; each grid cell is colored by the slice whose angular range
 * contains the cell's center, inside an outer radius (a donut leaves the
 * cells inside the inner radius blank). A legend is appended below the disk
 * when settings->show_legend is set. Returns an empty string for NULL slices,
 * count <= 0, invalid dimensions, or any non-positive/NaN slice value. */
CC_INLINE char* cc_pie_create(const cc_pie_slice_t* slices, int count,
                              int width, int height,
                              const cc_pie_settings_t* settings);

/* ============================ Histogram ============================
 * A histogram is a frequency distribution of a scalar sample sequence.
 * Unlike line/candle there is no OHLC data: the samples are grouped into
 * equal-width bins across a value window and each bin is drawn as a
 * vertical bar sampled at 8 sub-pixels per cell (cc_lower_eighth bar
 * tops, the same smoothness as the line chart). */

/* Histogram rendering options. Unspecified fields fall back to defaults:
 *   - rise_color : bar fill color                  (default green)
 *   - bg_color   : background of empty cells below a bar (default: none)
 *   - bin_count  : number of equal-width bins; <= 0 auto-selects (20 bins
 *                  for >= 40 samples, else 10, trimmed to the width)
 *   - min_value  : window minimum; NaN = the data minimum
 *   - max_value  : window maximum; NaN = the data maximum
 *   - show_bins  : append a value-axis footer row under the chart (window
 *                  min on the left, window max on the right)
 *   - show_prices: print the max-count / min-count labels in an 8-column
 *                  left margin (a vertical count axis)
 * Build with a designated initializer and pass NULL to any chart function
 * for full defaults. min_value/max_value use NaN as their "auto" sentinel,
 * but a partial {0} initializer (min==max==0.0) is ALSO auto, because a
 * valid explicit window always has min < max; any range where that does
 * not hold (NaN, equal, inverted) falls back to the data min/max so the
 * histogram auto-scales instead of collapsing to a single bar. */
typedef struct cc_hist_settings {
    const char* rise_color;
    const char* bg_color;
    int   bin_count;
    double min_value;
    double max_value;
    int   show_bins;
    int   show_prices;
} cc_hist_settings_t;

/* Renders a histogram of `count` samples into a `width` x `height` grid
 * (cells) and returns a malloc'd string (caller frees). Samples are counted
 * into equal-width bins across [min_value, max_value] — NaN endpoints
 * auto-select from the data range. Outliers are clamped into the edge bins
 * so a single huge value cannot empty every other bin, and a flat window
 * (min == max) is widened by +-1 so the mapping never divides by zero. Each
 * bar is scaled to the tallest bin and drawn with 8 sub-pixel rows
 * (cc_lower_eighth tops). Returns an empty string for NULL samples,
 * count <= 0, or invalid dimensions. */
CC_INLINE char* cc_hist_create(const double* samples, int count,
                               int width, int height,
                               const cc_hist_settings_t* settings);

/* ============================ Sparkline ============================
 * A sparkline is an ultra-compact, axis-less trend line drawn from a series
 * of close-like values. Unlike line/candle there is no OHLC data, no price
 * margin and no time footer — just a single (or few-row) line reusing the
 * line chart's 8-sub-pixel-per-cell smoothness (cc_lower_eighth). height is
 * expected small (default 1, up to ~3). */

/* Sparkline rendering options. Unspecified fields fall back to defaults:
 *   - rise_color : the single line / trend color   (default green)
 *   - area_color : faint fill under the line, or NULL for none (default)
 *   - min_above  : reserved sub-pixels at the TOP edge so the line does not
 *                  touch the top (default 0)
 *   - min_below  : reserved sub-pixels at the BOTTOM edge (default 0)
 * Build with a designated initializer and pass NULL to any chart function
 * for full defaults. min_above/min_below are plain ints, so a partial {0}
 * brace initializer gets the same defaults as NULL (0 sub-pixels reserved)
 * with no sentinel ambiguity; rise_color/area_color fall back per field. */
typedef struct cc_spark_settings {
    const char* rise_color;
    const char* area_color;
    int min_above;
    int min_below;
} cc_spark_settings_t;

/* Renders a sparkline of `count` samples into a `width` x `height` grid
 * (cells). Each column averages the samples in its span, exactly like the
 * line chart's downsampling, then maps the average to an 8-sub-pixel row;
 * heights are drawn with cc_lower_eighth tops and rows are assembled
 * top-to-bottom with no margin/footer/legend. An optional area_color fills
 * cells below the line. A flat (all-equal) series is centered exactly like
 * the line chart (no divide-by-zero). Returns a malloc'd string (caller
 * frees); invalid dimensions or NULL samples/count <= 0 yield an empty
 * string. Scratch buffers are heap-allocated and freed on every path. */
CC_INLINE char* cc_spark_create(const double* samples, int count,
                                int width, int height,
                                const cc_spark_settings_t* settings);

/* ============================= Bar chart =============================
 * A categorical bar chart draws an ordered list of (label, value) pairs as
 * vertical bars growing up from a shared zero baseline. Unlike the pie
 * (which normalizes slices to proportions) and the histogram (which bins raw
 * samples), each bar is drawn at its OWN height in proportion to the largest
 * value. Labels are categorical and are printed, when requested, in a footer
 * row below the plot (one label per output column, truncated to the column
 * width). */

/* One bar: a categorical label (may be NULL or empty) and a non-negative
 * height. Values are clamped to zero for the render (a negative value draws
 * that bar at zero height rather than below the axis; a future stage could
 * add a diverging below-baseline variant). Non-finite values are rejected
 * upstream (the wrapper and ABI), which keeps this header free of C99-only
 * `isfinite`. */
typedef struct cc_bar_item {
    const char* label;
    double       value;
} cc_bar_item_t;

/* Bar chart rendering options. Unspecified fields fall back to defaults:
 *   - rise_color : bar fill color                  (default green)
 *   - bg_color   : background of empty cells above a bar (default: none)
 *   - show_labels: 1 = append a footer row under the plot with each column's
 *                  label, truncated to the column width
 *   - show_prices: 1 = prepend an 8-column left value axis with the max bar
 *                  value at the top and 0 (the baseline) at the bottom
 * Build with a designated initializer and pass NULL to any chart function
 * for full defaults. All fields here are pointers or plain ints, so a
 * partial {0} brace initializer gets exactly the same defaults as NULL (no
 * double sentinels, so there is no brace-init ambiguity — unlike the
 * histogram's NaN window fields). */
typedef struct cc_bar_settings {
    const char* rise_color;
    const char* bg_color;
    int   show_labels;
    int   show_prices;
} cc_bar_settings_t;

/* Renders a bar chart of `count` (label, value) pairs into a `width` x
 * `height` grid (cells) and returns a malloc'd string (caller frees). Bars
 * grow up from a zero baseline, scaled so the largest value fills the full
 * height, and are drawn with 8 sub-pixel rows (cc_lower_eighth tops) like the
 * line chart. When width >= count each item maps to one or more columns;
 * when width < count the items are folded deterministically into the columns
 * (long-arithmetic index mapping, mirroring the histogram) and each column is
 * drawn at the MAX of the values in its span. An optional label footer
 * (show_labels) and 8-column value axis (show_prices) are assembled around
 * the grid. Returns a malloc'd string (caller frees); NULL items, count <= 0,
 * or invalid dimensions yield an empty string. Scratch buffers are
 * heap-allocated (no VLAs) and freed on every path. */
CC_INLINE char* cc_bar_create(const cc_bar_item_t* items, int count,
                              int width, int height,
                              const cc_bar_settings_t* settings);

/* ============================ Stacked bar chart ===========================
 * A stacked bar chart divides each category's total bar into vertical
 * segments, one per series: every category has the same set of series, and
 * its bar height is the SUM of the series' values for that category (a
 * part-of-whole view), unlike the plain bar where each item is its own full
 * height. All series share the same category count. Draws with 8 sub-pixel
 * rows (cc_lower_eighth tops) exactly like the bar chart; each cell is
 * colored by the series that sits on the stack's surface at that cell. */

/* One stacked-bar series: a name (may be NULL/empty — used only for
 * documentation; the per-series color comes from the palette) and a `values`
 * array with one entry per category. All series must share the same category
 * count (supplied by settings->cats). Values are clamped to zero for the
 * render (negative entries draw at zero height rather than below the axis);
 * non-finite values are rejected upstream (the wrapper and ABI), which keeps
 * this header free of C99-only `isfinite`. */
typedef struct cc_stack_series {
    const char*      name;
    const double*    values;
} cc_stack_series_t;

/* Stacked bar rendering options. Unspecified fields fall back to defaults:
 *   - colors    : a NULL-terminated array of ANSI color strings used as the
 *                 per-series palette, or NULL for the fixed default palette
 *   - bg_color  : background of empty cells above the tallest stack (default:
 *                 none)
 *   - cat_labels: optional array of `cats` category labels printed, when
 *                 show_labels is set, one per output column in the footer
 *                 (NULL = no category names)
 *   - series    : number of series (mirrors the `series_count` argument)
 *   - cats      : number of categories — the length of every series' values
 *                 array; REQUIRED (a NULL settings or cats <= 0 yields an
 *                 empty chart, since the values lengths cannot be derived
 *                 from the pointers alone)
 *   - show_labels: 1 = append a footer row under the plot with each column's
 *                  category label (from cat_labels), truncated to the column
 *                  width
 *   - show_prices: 1 = prepend an 8-column left value axis with the tallest
 *                  stack total at the top and 0 (the baseline) at the bottom
 * All fields are pointers or plain ints, so a partial {0} brace initializer
 * gets exactly the same defaults as NULL (no double sentinels, so there is
 * no brace-init ambiguity — unlike the histogram's NaN window fields). */
typedef struct cc_stack_settings {
    const char* const* colors;
    const char*        bg_color;
    const char* const* cat_labels;
    int   series;
    int   cats;
    int   show_labels;
    int   show_prices;
} cc_stack_settings_t;

/* Renders a stacked bar chart of `series_count` series, each with `cats`
 * values (settings->cats), into a `width` x `height` grid (cells) and returns
 * a malloc'd string (caller frees). Within every category the series' values
 * are summed into that category's total; each bar (category) is a vertical
 * stack of segments drawn bottom-to-top, one per series, proportional to each
 * series' value / the tallest stack total, and colored by the series via a
 * deterministic palette (or the settings->colors override). When width >=
 * cats each category maps to one or more columns; when width < cats the
 * categories are folded into the columns deterministically (long-arithmetic
 * index mapping, mirroring the bar chart) and each column aggregates its
 * categories by SUM (stacked bars are about totals). A footer of drop and
 * value-axis margin are assembled exactly like the bar chart. Returns a
 * malloc'd string (caller frees); NULL series, series_count <= 0, settings
 * NULL or cats <= 0, or invalid dimensions yield an empty string. Scratch
 * buffers are heap-allocated (no VLAs) and freed on every path. */
CC_INLINE char* cc_stack_create(const cc_stack_series_t* series, int series_count,
                                int width, int height,
                                const cc_stack_settings_t* settings);

/* ============================= Heatmap chart =============================
 * A heatmap draws a 2-D matrix of scalar values as a grid of colored cells:
 * every value maps to one of a fixed, deterministic ladder of ANSI colors by
 * its position between the matrix minimum and maximum. There is no OHLC
 * data — the whole chart is a grid of values. The height / width arguments
 * are the grid size in cells, and the matrix (rows x cols) is mapped onto
 * that grid (see the downsample / padding rules in cc_heat_create). */

/* Heatmap rendering options. Unspecified fields fall back to defaults:
 *   - low_color  : ANSI color for the minimum value (default bright-black,
 *                  a dim gray; ladder index 0)
 *   - high_color : ANSI color for the maximum value (default bright-white,
 *                  ladder index RAMP_LEN-1)
 *   - mid_color  : optional ANSI color that replaces the ladder's middle
 *                  entry (ladder index 5) for a 3-stop ramp; NULL (default)
 *                  leaves the fixed middle ramp color in place (2-stop)
 *   - bg_color   : color of the grid cells that the matrix does not cover
 *                  (matrix smaller than width/height), or NULL for none
 *   - row_labels : optional `rows` labels printed in a left margin when
 *                  show_labels is set, or NULL (no row margin)
 *   - col_labels : optional `cols` labels printed in a footer row under the
 *                  grid when show_labels is set, or NULL (no footer)
 *   - show_labels: 1 = print the row/col label frame around the grid
 * All fields are pointers or plain ints, so a partial {0} brace initializer
 * gets exactly the same defaults as NULL (no double sentinels, so there is
 * no brace-init ambiguity — unlike the histogram's NaN window fields). */
typedef struct cc_heat_settings {
    const char* low_color;
    const char* high_color;
    const char* mid_color;
    const char* bg_color;
    const char* const* row_labels;
    const char* const* col_labels;
    int   show_labels;
} cc_heat_settings_t;

/* The colormap ladder. This is the chart's determinism contract: a value's
 * ratio r in [0,1] ((value - data_min) / (data_max - data_min), 0.0 for an
 * all-equal matrix) selects color index (int)(r * (CC_HEAT_RAMP_LEN - 1)),
 * clamped into range. Index 0 is the low end and index CC_HEAT_RAMP_LEN-1
 * the high end; low_color/high_color substitute those two entries and an
 * optional mid_color substitutes index CC_HEAT_MID_INDEX. The ramp's ANSI
 * strings are fixed, so every binding reproduces the same bytes for the same
 * ratio. The full mapping, by ratio bucket (r in [0,1]):
 *
 *   r in [0/9, 1/9)   -> index 0  (low_color default bright-black)
 *   r in [1/9, 2/9)   -> index 1  (blue)
 *   r in [2/9, 3/9)   -> index 2  (cyan)
 *   r in [3/9, 4/9)   -> index 3  (bright-cyan)
 *   r in [4/9, 5/9)   -> index 4  (green)
 *   r in [5/9, 6/9)   -> index 5  (MID_INDEX; mid_color default yellow)
 *   r in [6/9, 7/9)   -> index 6  (bright-yellow)
 *   r in [7/9, 8/9)   -> index 7  (red)
 *   r in [8/9, 9/9)   -> index 8  (bright-red)
 *   r == 1 (exactly)  -> index 9  (high_color default bright-white)
 */
#define CC_HEAT_RAMP_LEN 10
#define CC_HEAT_MID_INDEX 5

/* Renders a heatmap of a `rows` x `cols` row-major `values` matrix into a
 * `width` x `height` grid (cells) and returns a malloc'd string (caller
 * frees). Each grid cell (gx, gy — gy counting from the TOP row) represents
 * a rectangular block of the matrix. A cell is "covered" when its block is
 * non-empty: if the matrix is LARGER than the grid (rows > height or cols >
 * width) the matrix is deterministically downsampled by BLOCK-AVERAGE (each
 * covered cell renders the mean of the matrix values in its span); if the
 * matrix is SMALLER than or equal to the grid, each matrix element occupies
 * its own cell in the top-left (row 0 / column 0) and the remaining cells
 * are background (bg_color or blank). Covered cells are colored by their
 * block-average value on the fixed ladder above; an all-equal matrix maps
 * every covered cell to ladder index 0 (a single color). Returns a malloc'd
 * string (caller frees); NULL values, rows <= 0, cols <= 0, or invalid
 * dimensions yield an empty string. Scratch buffers are heap-allocated (no
 * VLAs) and freed on every path. */
CC_INLINE char* cc_heat_create(const double* values, int rows, int cols,
                               int width, int height,
                               const cc_heat_settings_t* settings);

/* ================================ Box plot ===============================
 * A box plot draws a distribution per category: for each category its
 * samples' five-number summary [min, Q1, median, Q3, max] is rendered as a
 * vertical box (the Q1..Q3 interquartile region) with whiskers (the
 * min..max span) and the median drawn as a solid line. The statistics are
 * computed deterministically by the renderer from the raw samples (see the
 * nearest-rank convention in cc_box_create), so every binding reproduces
 * the same bytes for the same samples. There is no OHLC data — the input is
 * one array of samples per category. */

/* One category: a label (may be NULL/empty; labels are not printed by the
 * core — they exist for bindings' own footers) and its `samples` array of
 * `n` values. A category with n <= 0 or a NULL samples array makes the
 * WHOLE chart empty, because a box has no well-defined five-number summary
 * without any samples. */
typedef struct cc_box_category {
    const char* name;
    const double* samples;
    int n;
} cc_box_category_t;

/* Box plot rendering options. Unspecified fields fall back to defaults:
 *   - rise_color : ANSI color for the box (Q1..Q3) and the median line
 *                  (default green)
 *   - area_color : ANSI color for the whiskers (the vertical min..max line);
 *                  the empty string or NULL means the whiskers share
 *                  rise_color (the default)
 *   - bg_color   : color of the empty cells above/below each box, or NULL
 *                  for none
 *   - show_prices: 1 = prepend an 8-column value axis with the global max
 *                  on the top row and the global min on the bottom row
 * All fields are pointers or plain ints, so a partial {0} brace initializer
 * gets exactly the same defaults as NULL (no double sentinels, so there is
 * no brace-init ambiguity — unlike the histogram's NaN window fields). */
typedef struct cc_box_settings {
    const char* rise_color;
    const char* area_color;
    const char* bg_color;
    int show_prices;
} cc_box_settings_t;

/* Renders a box plot of `cat_count` categories into a `width` x `height`
 * grid and returns a malloc'd string (caller frees).
 *
 * QUARTILE CONVENTION (nearest-rank). For each category's n sorted samples
 * s[0..n-1]:
 *     min    = s[0]
 *     Q1     = s[floor((n-1) * 0.25)]
 *     median = s[floor((n-1) * 0.50)]
 *     Q3     = s[floor((n-1) * 0.75)]
 *     max    = s[n-1]
 * This is the nearest-rank method: a quartile is the value of the element
 * at the rounded-down rank, with no interpolation between elements. For
 * n == 1 all five collapse to the single sample.
 *
 * VALUE AXIS. The global minimum and maximum across every category's samples
 * define the value span. Each value v maps to a sub-pixel row — there are 8
 * sub-pixels per cell, height * 8 total — by
 *     level(v) = (gmax == gmin) ? (pixel_height / 2)
 *                                : floor((v - gmin) / (gmax - gmin) *
 *                                        (pixel_height - 1))
 * so the global min sits on the bottom sub-pixel (row 0) and the global max
 * on the top sub-pixel (row pixel_height - 1). Cell row r (from the bottom)
 * is level/8.
 *
 * COLUMN MAPPING. Output column w (0..width-1) draws the single category
 *     cs = floor(w * cat_count / width)
 * — the FIRST category in that column's long-arithmetic span. When
 * width >= cat_count a category whose span covers several columns is drawn
 * in each of them (repeated); when width < cat_count some categories are
 * skipped so every column still maps to exactly one category, always
 * deterministically.
 *
 * GLYPH MAPPING (per column, bottom row 0 .. top row height-1). Let lo =
 * level(min)/8, q1 = level(Q1)/8, md = level(median)/8, q3 = level(Q3)/8,
 * hi = level(max)/8 and LQ1 = level(Q1), LQ3 = level(Q3). A cell row r is
 * drawn with the first matching rule:
 *   1. Median line  (r == md):                 full block `█` in rise_color
 *      — always the full block, even when the median aliases the Q3 top
 *      edge, so the median is a solid horizontal line (the emphasis)
 *   2. Box interior (q1 < r < q3):             full block `█` in rise_color
 *   3. Box top edge (r == q3):                 `cc_lower_eighth(h)` in
 *      rise_color, h = (LQ3 % 8) + 1 — the Q3 sub-pixel, so the top edge is
 *      sub-pixel precise (h == 8 renders a full block)
 *   4. Box bottom edge (r == q1, q1 < q3):     full block `█` in rise_color
 *      — the Q1 sub-pixel floor rounds down to a full cell; when q1 == q3
 *      (box within one row) it is `cc_lower_eighth(LQ3 - LQ1 + 1)`
 *   5. Whisker       (lo <= r <= hi, not a box row): vertical `│`
 *      (CC_LINE_VERTICAL) in area_color (default rise_color)
 *   6. Background:                             space, or bg_color
 * Partial top edges reuse cc_lower_eighth; the whiskers reach from the
 * global-min row to the global-max row.
 *
 * Returns a malloc'd string (caller frees); NULL cats, cat_count <= 0, any
 * category with n <= 0 / NULL samples, or invalid dimensions yield an empty
 * string. Scratch buffers are heap-allocated (no VLAs) and freed on every
 * path. NaN/inf samples are rejected upstream (wrapper/ABI), so this header
 * stays free of C99-only isfinite. */
CC_INLINE char* cc_box_create(const cc_box_category_t* cats, int cat_count,
                              int width, int height,
                              const cc_box_settings_t* settings);

#ifdef __cplusplus
}
#endif

#ifdef CCHARTS_IMPLEMENTATION

/* ============================ Public constants ============================ */
/* (CC_MAX_DIM / CC_MAX_CELLS are defined above, outside the implementation
 * block, so the Python wrapper and other clients can also use them.) */

/* One candlestick. `timestamp` is epoch seconds, 0 when unknown. */
struct cc_ohlc {
    double open;
    double high;
    double low;
    double close;
    long long timestamp;
};

CC_INLINE cc_settings_t cc_settings_resolve(const cc_settings_t* settings) {
    cc_settings_t resolved = {
        .rise_color   = (settings && settings->rise_color)   ? settings->rise_color   : CC_COLOR_GREEN,
        .fall_color   = (settings && settings->fall_color)   ? settings->fall_color   : CC_COLOR_RED,
        .bg_color     = (settings && settings->bg_color)     ? settings->bg_color     : NULL,
        .area_color   = (settings && settings->area_color)   ? settings->area_color   : NULL,
        .single_color = settings ? settings->single_color : 0,
        .show_prices  = settings ? settings->show_prices  : 0,
        .show_times   = settings ? settings->show_times   : 0,
    };
    return resolved;
}

/* ============================ Internal helpers ============================
 * Everything below CCHARTS_IMPLEMENTATION is private. The pieces build on
 * each other in this order:
 *   1. cc_settings_resolve  -> normalize settings to full defaults
 *   2. capacity/dim helpers -> allocation and dimension sanity checks
 *   3. parsing helpers      -> whitespace trim + timestamp conversion
 *   4. cc_str_to_ohlc       -> CSV  -> cc_ohlc_t[]
 *   5. JSON mini-scanner    -> JSON -> cc_ohlc_t[]
 *   6. rendering helpers    -> value-to-pixel math + cell string builders
 *   7. cc_line_create       -> line chart renderer
 *   8. cc_candle_create     -> candle chart renderer
 * ========================================================================== */

/* Validates chart dimensions against the public limits. */
CC_INLINE int cc_dim_ok(int width, int height) {
    return width > 0 && height > 0 &&
           width <= CC_MAX_DIM && height <= CC_MAX_DIM &&
           (long long)width * (long long)height <= CC_MAX_CELLS;
}

CC_INLINE char* cc_trim_whitespace(char* str) {
    if (str == NULL || *str == '\0') {
        return str;
    }

    while (*str != '\0' && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    char* end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}

/* Days in a month, leap-year aware (used to validate ISO8601 input). */
CC_INLINE int cc_days_in_month(int year, int month) {
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

CC_INLINE long long cc_iso8601_to_epoch(const char* s) {
    /* Parses "YYYY-MM-DDTHH:MM:SS[+ZZ:ZZ|+ZZZZ|+ZZ|Z]" and converts the
     * wall-clock time to epoch seconds using Howard Hinnant's civil_date
     * algorithm, applying the UTC offset:
     *     epoch = civil_days(wall) * 86400 + seconds(wall) - offset
     * A missing offset (or "Z") is UTC. Returns 0 for malformed input. */
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0, consumed = 0;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d%n", &y, &mo, &d, &h, &mi, &se, &consumed) < 6) {
        return 0;
    }
    if (mo < 1 || mo > 12 || d < 1 || d > cc_days_in_month(y, mo) ||
        h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 60) {
        return 0;
    }

    long long offset = 0;
    const char* tz = s + consumed;
    if (*tz == 'Z' || *tz == 'z') {
        tz++;
    } else if (*tz == '+' || *tz == '-') {
        int oh = 0, om = 0, n2 = 0;
        int sign = (*tz == '-') ? -1 : 1;
        if (sscanf(tz + 1, "%d:%d%n", &oh, &om, &n2) == 2) {
            /* "+HH:MM" */
        } else if (sscanf(tz + 1, "%d%n", &oh, &n2) == 1) {
            if (n2 == 4) { om = oh % 100; oh = oh / 100; }   /* "+HHMM" */
            else if (n2 == 2) { om = 0; }                     /* "+HH" */
            else return 0;
        } else {
            return 0;
        }
        if (oh > 23 || om > 59) return 0;
        offset = sign * ((long long)oh * 3600 + (long long)om * 60);
        tz += 1 + n2;
    }
    if (*tz != '\0') return 0;   /* trailing garbage after the offset */

    long long yy = y - (mo <= 2);
    long long era = (yy >= 0 ? yy : yy - 399) / 400;
    long long yoe = yy - era * 400;
    long long doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + doe - 719468;

    return days * 86400 + (long long)h * 3600 + (long long)mi * 60 + se - offset;
}

CC_INLINE long long cc_parse_ts(const char* s) {
    /* Accepts either an ISO8601 string ("2026-07-20T21:00:00+03:00") or a
     * plain epoch-seconds number (possibly negative), and returns epoch
     * seconds (0 = none). ISO detection looks for the exact YYYY-MM-DD
     * prefix so a negative number like "-500" cannot be mistaken for one. */
    if (s == NULL || *s == '\0') {
        return 0;
    }
    size_t len = strlen(s);
    if (len >= 10 && s[4] == '-' && s[7] == '-' &&
        s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' &&
        s[2] >= '0' && s[2] <= '9' && s[3] >= '0' && s[3] <= '9') {
        return cc_iso8601_to_epoch(s);
    }
    return (long long)atoll(s);
}

/* ------------------------------ CSV parser ------------------------------ */

CC_INLINE int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc,
                             char val_seperator, char line_seperator)
{
    /* Splits `data` on `line_seperator` (at most `size` lines) and parses
     * up to 5 `val_seperator`-separated fields per line:
     * open,high,low,close[,timestamp]. Empty lines are skipped. Each line
     * and field is heap/slice-based, so there is no fixed-size buffer that
     * a long line could overflow. */
    if (data == NULL || ohlc == NULL || size <= 0) {
        return 1;
    }

    *ohlc = (cc_ohlc_t*)calloc((size_t)size, sizeof(cc_ohlc_t));
    if (*ohlc == NULL) {
        fprintf(stderr, "ccharts: memory allocation failed\n");
        return 1;
    }

    const char* line_start = data;
    int idx = 0;

    while (*line_start != '\0' && idx < size) {
        const char* eol = strchr(line_start, line_seperator);
        size_t len = eol ? (size_t)(eol - line_start) : strlen(line_start);

        if (len > 0) {
            char* line = (char*)malloc(len + 1);
            if (line == NULL) {
                free(*ohlc);
                *ohlc = NULL;
                return 1;
            }
            memcpy(line, line_start, len);
            line[len] = '\0';

            char* tok = cc_trim_whitespace(line);
            if (*tok != '\0') {
                cc_ohlc_t o = {0};
                int field = 0;
                char* field_start = tok;
                while (field < 5) {
                    char* field_end = strchr(field_start, val_seperator);
                    if (field_end != NULL) *field_end = '\0';

                    char* f = cc_trim_whitespace(field_start);
                    if (field == 4) {
                        o.timestamp = cc_parse_ts(f);
                    } else {
                        double v = atof(f);
                        if (field == 0)      o.open = v;
                        else if (field == 1) o.high = v;
                        else if (field == 2) o.low = v;
                        else if (field == 3) o.close = v;
                    }
                    field++;
                    if (field_end == NULL) break;
                    field_start = field_end + 1;
                }
                (*ohlc)[idx++] = o;
            }
            free(line);
        }

        if (eol == NULL) break;
        line_start = eol + 1;
    }

    return 0;
}

/* ------------------------------ JSON parser ------------------------------
 * A small iterative scanner for the fixed schema (no recursion, no strstr):
 * string literals are the only things that can hide braces or commas, and
 * every value is skipped by structure instead of by substring search, so
 * content like "open" inside a string value can never be misread as a key.
 * ------------------------------------------------------------------------- */

/* Skips JSON whitespace. */
CC_INLINE void cc_json_skip_ws(const char** p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

/* Parses a JSON string literal at *p into out (max n-1 chars + NUL) and
 * advances *p past the closing quote. Escape sequences are consumed but not
 * decoded (the fixed schema never needs them). Returns 0 on malformed input. */
CC_INLINE int cc_json_parse_string(const char** p, char* out, size_t n) {
    const char* s = *p;
    if (*s != '"') return 0;
    s++;
    size_t k = 0;
    for (;;) {
        if (*s == '\0') return 0;
        if (*s == '\\') {
            s++;
            if (*s == '\0') return 0;
            if (*s == 'u') {                /* consume \uXXXX */
                for (int i = 0; i < 4; i++) { s++; if (*s == '\0') return 0; }
            }
            s++;
            continue;
        }
        if (*s == '"') break;
        if (k + 1 < n) out[k++] = *s;
        s++;
    }
    out[k] = '\0';
    *p = s + 1;
    return 1;
}

/* Skips a complete JSON value (string, number, literal, or nested
 * object/array) at *p and advances past it. Iterative — no recursion. */
CC_INLINE int cc_json_skip_value(const char** p) {
    const char* s = *p;
    cc_json_skip_ws(&s);
    if (*s == '\0') return 0;

    if (*s == '"') {
        s++;
        while (*s != '"') {
            if (*s == '\0') return 0;
            if (*s == '\\') { s++; if (*s == '\0') return 0; }
            s++;
        }
        s++;
        *p = s;
        return 1;
    }

    if (*s == '{' || *s == '[') {
        int depth = 0;
        for (;;) {
            if (*s == '\0') return 0;
            if (*s == '"') {
                s++;
                while (*s != '"') {
                    if (*s == '\0') return 0;
                    if (*s == '\\') { s++; if (*s == '\0') return 0; }
                    s++;
                }
            } else if (*s == '{' || *s == '[') {
                depth++;
            } else if (*s == '}' || *s == ']') {
                depth--;
                if (depth == 0) { s++; break; }
            }
            s++;
        }
        *p = s;
        return 1;
    }

    /* Bare token: number / true / false / null. */
    while (*s != '\0' && *s != ',' && *s != '}' && *s != ']' &&
           *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') s++;
    if (s == *p) return 0;   /* nothing consumed */
    *p = s;
    return 1;
}

/* Copies a bare JSON number token starting at *p into out (max n-1 chars)
 * and advances *p past it. Returns 0 when no number is present. */
CC_INLINE int cc_json_read_number_token(const char** p, char* out, size_t n) {
    const char* s = *p;
    size_t k = 0;
    while (*s == '-' || *s == '+' || (*s >= '0' && *s <= '9') ||
           *s == '.' || *s == 'e' || *s == 'E') {
        if (k + 1 < n) out[k++] = *s;
        s++;
    }
    if (k == 0) return 0;
    out[k] = '\0';
    *p = s;
    return 1;
}

CC_INLINE int cc_json_to_ohlc(const char* json, cc_ohlc_t** ohlc, int* size) {
    *ohlc = NULL;
    *size = 0;
    if (json == NULL) {
        return 1;
    }

    const char* p = json;
    cc_json_skip_ws(&p);
    if (*p != '[') return 1;
    p++;
    cc_json_skip_ws(&p);

    /* Pass 1: count the objects in the top-level array. */
    int count = 0;
    if (*p != ']') {
        for (;;) {
            cc_json_skip_ws(&p);
            if (*p != '{') return 1;
            int depth = 0;
            for (;;) {   /* skim to the matching '}' */
                if (*p == '\0') return 1;
                if (*p == '"') {
                    p++;
                    while (*p != '"') {
                        if (*p == '\0') return 1;
                        if (*p == '\\') { p++; if (*p == '\0') return 1; }
                        p++;
                    }
                } else if (*p == '{') {
                    depth++;
                } else if (*p == '}') {
                    depth--;
                    if (depth == 0) { p++; break; }
                }
                p++;
            }
            count++;
            cc_json_skip_ws(&p);
            if (*p == ',') { p++; continue; }
            if (*p == ']') break;
            return 1;
        }
    }
    if (count <= 0) return 1;

    *ohlc = (cc_ohlc_t*)calloc((size_t)count, sizeof(cc_ohlc_t));
    if (*ohlc == NULL) {
        fprintf(stderr, "ccharts: memory allocation failed\n");
        return 1;
    }

    /* Pass 2: parse each object into a cc_ohlc_t. */
    p = json;
    cc_json_skip_ws(&p);
    p++; /* '[' */
    cc_json_skip_ws(&p);
    int idx = 0;
    int valid = 1;
    if (*p != ']') {
        for (;;) {
            cc_json_skip_ws(&p);
            if (*p != '{') { valid = 0; break; }
            p++;
            cc_ohlc_t o = {0};
            for (;;) {
                cc_json_skip_ws(&p);
                if (*p == '}') { p++; break; }
                if (*p != '"') { valid = 0; break; }
                char key[16];
                if (!cc_json_parse_string(&p, key, sizeof(key))) { valid = 0; break; }
                cc_json_skip_ws(&p);
                if (*p != ':') { valid = 0; break; }
                p++; /* ':' */
                cc_json_skip_ws(&p);

                if (strcmp(key, "ts") == 0) {
                    char sval[64];
                    if (*p == '"') {
                        if (!cc_json_parse_string(&p, sval, sizeof(sval))) { valid = 0; break; }
                    } else {
                        if (!cc_json_read_number_token(&p, sval, sizeof(sval))) { valid = 0; break; }
                    }
                    o.timestamp = cc_parse_ts(sval);
                } else if (strcmp(key, "open") == 0 || strcmp(key, "high") == 0 ||
                           strcmp(key, "low") == 0 || strcmp(key, "close") == 0) {
                    char nval[32];
                    if (!cc_json_read_number_token(&p, nval, sizeof(nval))) { valid = 0; break; }
                    double v = atof(nval);
                    if (strcmp(key, "open") == 0)      o.open = v;
                    else if (strcmp(key, "high") == 0) o.high = v;
                    else if (strcmp(key, "low") == 0)  o.low = v;
                    else                               o.close = v;
                } else {
                    /* Unknown key — "volume" and friends are skipped. */
                    if (!cc_json_skip_value(&p)) { valid = 0; break; }
                }

                cc_json_skip_ws(&p);
                if (*p == ',') { p++; continue; }
                if (*p == '}') { p++; break; }
                valid = 0;
                break;
            }
            if (!valid) break;
            (*ohlc)[idx++] = o;
            cc_json_skip_ws(&p);
            if (*p == ',') { p++; continue; }
            if (*p == ']') break;
            valid = 0;
            break;
        }
    }
    if (!valid) {
        free(*ohlc);
        *ohlc = NULL;
        return 1;
    }

    *size = idx;
    return 0;
}

/* --------------------------- Rendering helpers --------------------------- */

/* Smallest / largest value in arr[0..n-1]. */
CC_INLINE double find_min(double arr[], int n) {
    double min = arr[0];
    for (int i = 1; i < n; i++) {
        if (arr[i] < min) {
            min = arr[i];
        }
    }
    return min;
}

CC_INLINE double find_max(double arr[], int n) {
    double max = arr[0];
    for (int i = 1; i < n; i++) {
        if (arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}

/* Maps a value in [min, max] to a pixel row in [0, pixel_height-1].
 * `range` is max - min; a flat range is centered to avoid divide-by-zero. */
CC_INLINE int cc_pixel(double val, double min, double max, double range, int pixel_height) {
    /* `max` is kept for call-site symmetry (every caller has min/max/range at
     * hand); the mapping only needs min and range. Silences -Wextra for the
     * bindings, which compile this header with their own flags. */
    (void)max;
    double t = (range == 0.0) ? 0.5 : (val - min) / range;
    int p = (int)lround(t * (pixel_height - 1));
    if (p < 0) p = 0;
    if (p >= pixel_height) p = pixel_height - 1;
    return p;
}

/* NULL as an empty string, for the printf calls below. */
#define CC_S(p) ((p) != NULL ? (p) : "")

/* The reset sequence is only needed when something was actually colored.
 * A color set to the empty string means "emit no escape", so a chart whose
 * colors are all empty comes out as plain text with no escapes at all —
 * which is what lets the output go somewhere that is not a terminal. */
CC_INLINE const char* cc_reset_for(const char* a, const char* b) {
    int colored = (a != NULL && *a != '\0') || (b != NULL && *b != '\0');
    return colored ? CC_COLOR_RESET : "";
}

CC_INLINE void cc_render_cell(char out[32], unsigned char bodybits, unsigned char wickbits, const char* fg, const char* bg) {
    /* Turns a candle cell's mask bits into a ready-to-print string.
     * bodybits: bit0 = lower half filled, bit1 = upper half filled (body).
     * wickbits: non-zero means a wick passes through this cell. Body wins
     * over wick so a partial body still reads as a solid block. */
    const char* block;
    if (bodybits == 3)          block = CC_BLOCK_FULL;
    else if (bodybits == 2)     block = CC_BLOCK_UPPER_HALF;
    else if (bodybits == 1)     block = CC_BLOCK_LOWER_HALF;
    else if (wickbits != 0)     block = CC_LINE_VERTICAL;
    else                        block = NULL;

    if (block != NULL) {
        snprintf(out, 32, "%s%s%s%s", CC_S(bg), CC_S(fg), block,
                 cc_reset_for(bg, fg));
    } else if (bg != NULL) {
        snprintf(out, 32, "%s %s", bg, cc_reset_for(bg, NULL));
    } else {
        snprintf(out, 32, " ");
    }
}

CC_INLINE cc_ohlc_t cc_agg_ohlc(const cc_ohlc_t* data, int start, int end) {
    /* Aggregates candles [start, end) into one virtual candle:
     * open = first open, high = max high, low = min low, close = last close.
     * Used to compress many candles into a few columns (width < size). */
    cc_ohlc_t v = { .open = data[start].open, .high = data[start].high,
                    .low = data[start].low, .close = data[end - 1].close };
    for (int k = start + 1; k < end; k++) {
        if (data[k].high > v.high) v.high = data[k].high;
        if (data[k].low < v.low) v.low = data[k].low;
    }
    return v;
}

CC_INLINE const char* cc_lower_eighth(int n) {
    /* Returns the lower 1/8 block whose bar height is n/8 (n in 1..8).
     * These characters give the line chart its 8-level vertical resolution:
     * ▁▂▃▄▅▆▇█. */
    static const char* table[] = {
        CC_BLOCK_LOWER_1_8, CC_BLOCK_LOWER_2_8, CC_BLOCK_LOWER_3_8, CC_BLOCK_LOWER_HALF,
        CC_BLOCK_LOWER_5_8, CC_BLOCK_LOWER_6_8, CC_BLOCK_LOWER_7_8, CC_BLOCK_FULL
    };
    if (n < 1) n = 1;
    if (n > 8) n = 8;
    return table[n - 1];
}

CC_INLINE const char* cc_time_format(long long first, long long last, int count) {
    /* Picks a strftime format from the average interval between candles:
     *   >= 20 hours (daily/weekly/monthly)  -> "YYYY-MM-DD"
     *   intraday spanning > 1 day           -> "MM-DD HH:MM"
     *   intraday within one day             -> "HH:MM" */
    long long span = last - first;
    long long interval = (count > 1) ? span / (count - 1) : 0;
    if (interval >= 72000) {
        return "%Y-%m-%d";
    }
    if (span >= 86400) {
        return "%m-%d %H:%M";
    }
    return "%H:%M";
}

CC_INLINE char* cc_assemble_chart(int width, int height, char* columns,
                                  const cc_settings_t* s,
                                  long long ts_first, long long ts_last, int count,
                                  double max, double min) {
    /* Joins the per-cell strings (columns[(x*height + y)*32 .. +32), y
     * counted from the bottom) into the final chart string, printing rows
     * top-to-bottom. Optionally adds a left price margin (max on the top
     * row, min on the bottom row) and a footer with the first/last
     * timestamps. Allocates the result with a pointer cursor (no strcat
     * re-scans); the caller frees it. */
    int margin = 0;
    char max_label[16] = "";
    char min_label[16] = "";
    if (s->show_prices) {
        margin = 8;
        snprintf(max_label, sizeof(max_label), "%.2f", max);
        snprintf(min_label, sizeof(min_label), "%.2f", min);
    }

    char t_first[32] = "";
    char t_last[32] = "";
    int footer = 0;
    if (s->show_times && count > 0 && ts_first > 0 && ts_last > 0) {
        const char* fmt = cc_time_format(ts_first, ts_last, count);
        time_t a = (time_t)ts_first;
        time_t b = (time_t)ts_last;
        struct tm tmv;
        if (CC_GMTIME_R(&a, &tmv)) strftime(t_first, sizeof(t_first), fmt, &tmv);
        if (CC_GMTIME_R(&b, &tmv)) strftime(t_last, sizeof(t_last), fmt, &tmv);
        footer = 1;
    }

    const int line_len = width + margin;
    const int rows = height + footer;
    /* Bounded by CC_MAX_CELLS: width*height <= 1e6, so this is <= ~32 MB. */
    size_t total_size = (size_t)line_len * (size_t)rows * 32 + (size_t)rows + 1;
    char* chart = (char*)calloc(total_size, 1);
    if (chart == NULL) return NULL;

    char* w = chart;
    for (int y = height - 1; y >= 0; y--) {
        if (margin > 0) {
            char mlabel[16] = "";
            if (y == height - 1) snprintf(mlabel, sizeof(mlabel), "%8s", max_label);
            else if (y == 0)     snprintf(mlabel, sizeof(mlabel), "%8s", min_label);
            else { memset(mlabel, ' ', 8); mlabel[8] = '\0'; }
            size_t ml = strlen(mlabel);
            memcpy(w, mlabel, ml);
            w += ml;
        }
        for (int x = 0; x < width; x++) {
            const char* cell = columns + ((size_t)x * height + (size_t)y) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }

    if (footer) {
        for (int i = 0; i < line_len; i++) *w++ = ' ';
        size_t t1 = strlen(t_first);
        if (t1 > (size_t)line_len) t1 = (size_t)line_len;
        size_t t2 = strlen(t_last);
        if (t2 > (size_t)line_len) t2 = (size_t)line_len;
        if (t1 > 0) memcpy(w - line_len, t_first, t1);
        if (t2 > 0) memcpy(w - t2, t_last, t2);
        *w++ = '\n';
    }
    *w = '\0';
    return chart;
}

/* ------------------------------ Line renderer ------------------------------
 * For each output column, average the closes that fall into that column
 * (like the candle chart, when width < size several candles share a column).
 * The column's height is snapped to a cell boundary on the bottom and drawn
 * with an 8-level bar on top (cc_lower_eighth), so the line's upper edge is
 * smooth. Adjacent columns share pixels to avoid gaps. With area_color set,
 * cells below the line are filled. Colors come from cc_settings_t: either
 * one color for the whole chart (single_color) or per segment.
 * All scratch buffers are heap-allocated (no VLAs) and freed on every path.
 * ------------------------------------------------------------------------- */

CC_INLINE char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height,
                               const cc_settings_t* settings) {
    if (data == NULL || size <= 0 || !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }

    cc_settings_t s = cc_settings_resolve(settings);

    double* closes = (double*)malloc((size_t)size * sizeof(double));
    double* vals = (double*)calloc((size_t)width, sizeof(double));
    const char** col_color = (const char**)malloc((size_t)width * sizeof(char*));
    int* py = (int*)malloc((size_t)width * sizeof(int));
    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (closes == NULL || vals == NULL || col_color == NULL ||
        py == NULL || columns == NULL) {
        free(closes); free(vals); free(col_color); free(py); free(columns);
        return NULL;
    }

    for (int i = 0; i < size; i++) {
        closes[i] = data[i].close;
    }

    for (int w = 0; w < width; w++) {
        int start_idx = (int)(((long long)w * size) / width);
        int end_idx = (int)((((long long)w + 1) * size) / width);
        if (end_idx <= start_idx) end_idx = start_idx + 1;

        double sum = 0.0;
        for (int k = start_idx; k < end_idx && k < size; k++) {
            sum += closes[k];
        }
        vals[w] = sum / (end_idx - start_idx);
    }

    double min = find_min(vals, width);
    double max = find_max(vals, width);
    double diff_range = max - min;

    if (s.single_color) {
        double change = (size > 1) ? (closes[size-1] - closes[size-2]) : 0.0;
        const char* color = (change >= 0.0) ? s.rise_color : s.fall_color;
        for (int w = 0; w < width; w++) {
            col_color[w] = color;
        }
    } else {
        for (int w = 0; w < width; w++) {
            double prev = (w > 0) ? vals[w-1] : vals[w];
            col_color[w] = (vals[w] >= prev) ? s.rise_color : s.fall_color;
        }
    }

    int pixel_height = height * 8;

    for (int i = 0; i < width; i++) {
        py[i] = cc_pixel(vals[i], min, max, diff_range, pixel_height);
    }

    for (int i = 0; i < width; i++) {
        int lo = py[i];
        int hi = py[i];
        if (i + 1 < width) {
            if (py[i+1] < lo) lo = py[i+1];
            if (py[i+1] > hi) hi = py[i+1];
        }

        int cell_lo = lo / 8;
        int cell_hi = hi / 8;
        char* col = columns + (size_t)i * height * 32;

        for (int r = 0; r < cell_lo && r < height; r++) {
            char* cell = col + (size_t)r * 32;
            if (s.area_color != NULL) {
                snprintf(cell, 32, "%s %s", s.area_color,
                         cc_reset_for(s.area_color, NULL));
            } else if (s.bg_color != NULL) {
                snprintf(cell, 32, "%s %s", s.bg_color,
                         cc_reset_for(s.bg_color, NULL));
            } else {
                snprintf(cell, 32, " ");
            }
        }

        for (int r = cell_lo; r < cell_hi && r < height; r++) {
            snprintf(col + (size_t)r * 32, 32, "%s%s%s", CC_S(col_color[i]),
                     CC_BLOCK_FULL, cc_reset_for(col_color[i], NULL));
        }

        if (cell_hi >= 0 && cell_hi < height) {
            int count = hi - cell_hi * 8 + 1;
            if (count > 8) count = 8;
            snprintf(col + (size_t)cell_hi * 32, 32, "%s%s%s",
                     CC_S(col_color[i]), cc_lower_eighth(count),
                     cc_reset_for(col_color[i], NULL));
        }

        for (int r = cell_hi + 1; r < height; r++) {
            char* cell = col + (size_t)r * 32;
            if (s.bg_color != NULL) {
                snprintf(cell, 32, "%s %s", s.bg_color,
                         cc_reset_for(s.bg_color, NULL));
            } else {
                snprintf(cell, 32, " ");
            }
        }
    }

    long long ts_first = data[0].timestamp;
    long long ts_last = data[size-1].timestamp;
    char* chart = cc_assemble_chart(width, height, columns, &s,
                                    ts_first, ts_last, size, max, min);
    free(closes); free(vals); free(col_color); free(py); free(columns);
    return chart;
}

/* ----------------------------- Candle renderer -----------------------------
 * Two modes:
 *   - width >= size : each candle owns several columns; the body (open-close)
 *     is drawn as blocks, the wick (high-low) as a thin │ in the center
 *     column, and the last column of each candle is left empty as a gap.
 *   - width < size  : neighboring candles are collapsed into virtual candles
 *     (cc_agg_ohlc), one per column.
 * The vertical price range covers the whole high/low span so wicks never
 * clip. Rises use rise_color, falls use fall_color (close >= open).
 * All scratch buffers are heap-allocated (no VLAs) and freed on every path.
 * ------------------------------------------------------------------------- */

CC_INLINE char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height,
                                 const cc_settings_t* settings) {
    if (data == NULL || size <= 0 || !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }

    cc_settings_t s = cc_settings_resolve(settings);
    int pixel_height = height * 2;

    double min = data[0].low;
    double max = data[0].high;
    for (int i = 1; i < size; i++) {
        if (data[i].low < min)  min = data[i].low;
        if (data[i].high > max) max = data[i].high;
    }
    double range = max - min;

    size_t cells = (size_t)width * (size_t)height;
    unsigned char* body_mask = (unsigned char*)calloc(cells, 1);
    unsigned char* wick_mask = (unsigned char*)calloc(cells, 1);
    const char** col_color = (const char**)malloc((size_t)width * sizeof(char*));
    char* columns = (char*)malloc(cells * 32);
    if (body_mask == NULL || wick_mask == NULL || col_color == NULL || columns == NULL) {
        free(body_mask); free(wick_mask); free(col_color); free(columns);
        return NULL;
    }

    if (width >= size) {

        for (int i = 0; i < size; i++) {
            int cs = (int)(((long long)i * width) / size);
            int ce = (int)((((long long)i + 1) * width) / size);
            if (ce <= cs) ce = cs + 1;
            int gap = (ce - cs >= 2) ? (ce - 1) : ce;
            int cmid = cs + (gap - cs - 1) / 2;

            const char* color = (data[i].close >= data[i].open) ? s.rise_color : s.fall_color;

            int po = cc_pixel(data[i].open, min, max, range, pixel_height);
            int pc = cc_pixel(data[i].close, min, max, range, pixel_height);
            int ph = cc_pixel(data[i].high, min, max, range, pixel_height);
            int pl = cc_pixel(data[i].low, min, max, range, pixel_height);

            int blo = (po < pc) ? po : pc;
            int bhi = (po > pc) ? po : pc;

            for (int c = cs; c < ce; c++) {
                col_color[c] = color;
                if (c < gap) {
                    unsigned char* bm = body_mask + (size_t)c * height;
                    for (int p = blo; p <= bhi; p++) {
                        bm[p / 2] |= (unsigned char)((p % 2) ? 2 : 1);
                    }
                }
            }

            unsigned char* wm = wick_mask + (size_t)cmid * height;
            for (int p = pl; p <= ph; p++) {
                wm[p / 2] |= (unsigned char)((p % 2) ? 2 : 1);
            }
        }
    } else {

        for (int w = 0; w < width; w++) {
            int ss = (int)(((long long)w * size) / width);
            int se = (int)((((long long)w + 1) * size) / width);
            if (se <= ss) se = ss + 1;

            cc_ohlc_t v = cc_agg_ohlc(data, ss, se);
            col_color[w] = (v.close >= v.open) ? s.rise_color : s.fall_color;

            int po = cc_pixel(v.open, min, max, range, pixel_height);
            int pc = cc_pixel(v.close, min, max, range, pixel_height);
            int ph = cc_pixel(v.high, min, max, range, pixel_height);
            int pl = cc_pixel(v.low, min, max, range, pixel_height);

            int blo = (po < pc) ? po : pc;
            int bhi = (po > pc) ? po : pc;

            unsigned char* bm = body_mask + (size_t)w * height;
            for (int p = blo; p <= bhi; p++) {
                bm[p / 2] |= (unsigned char)((p % 2) ? 2 : 1);
            }
            unsigned char* wm = wick_mask + (size_t)w * height;
            for (int p = pl; p <= ph; p++) {
                wm[p / 2] |= (unsigned char)((p % 2) ? 2 : 1);
            }
        }
    }

    for (int i = 0; i < width; i++) {
        char* col = columns + (size_t)i * height * 32;
        for (int j = 0; j < height; j++) {
            cc_render_cell(col + (size_t)j * 32, body_mask[(size_t)i * height + j],
                           wick_mask[(size_t)i * height + j], col_color[i], s.bg_color);
        }
    }

    long long ts_first = data[0].timestamp;
    long long ts_last = data[size-1].timestamp;
    char* chart = cc_assemble_chart(width, height, columns, &s,
                                    ts_first, ts_last, size, max, min);
    free(body_mask); free(wick_mask); free(col_color); free(columns);
    return chart;
}

/* ------------------------------- Pie renderer ------------------------------
 * A pie normalizes slice values to angles (value/total * 2*pi) and samples
 * each grid cell once at its center in sub-pixel space (8 sub-pixels per
 * cell, like the line chart, so the disk is round in pixel space). A cell is
 * colored by the slice whose angular range holds its center, inside an outer
 * radius; a donut additionally blanks the cells inside the inner radius.
 * Colors come from a fixed deterministic palette or a NULL-terminated
 * per-slice override. The legend, when requested, is appended below the disk,
 * one "label  value (pct%)" line per slice. All scratch buffers are
 * heap-allocated (no VLAs) and freed on every path.
 * -------------------------------------------------------------------------- */

#define CC_PI 3.14159265358979323846    /* M_PI is not portable C89/C99 */
#define CC_PIE_DONUT_RADIUS_RATIO 0.5   /* inner radius = this * outer radius */
/* Terminal character cells are roughly twice as tall as they are wide
 * (~2:1). The pie renderer draws the disk in square sub-pixel space (8x8 per
 * cell), so an unscaled circle would read as a stretched ellipse on screen.
 * CC_PIE_ASPECT is the terminal cell aspect (width / height, ~0.5): the
 * renderer compresses the horizontal axis by this factor when testing
 * distance and slice angles, so the disk renders as a round circle. */
#define CC_PIE_ASPECT 0.5               /* terminal cell width / height */

/* Deterministic fixed palette. Never random — a random palette would break
 * the byte-for-byte conformance goldens. Indexed per slice (mod length). */
static const char* const CC_PIE_DEFAULT_PALETTE[] = {
    CC_COLOR_BLUE, CC_COLOR_GREEN, CC_COLOR_YELLOW, CC_COLOR_RED,
    CC_COLOR_MAGENTA, CC_COLOR_CYAN, CC_COLOR_BRIGHT_BLUE,
    CC_COLOR_BRIGHT_GREEN, CC_COLOR_BRIGHT_YELLOW, CC_COLOR_BRIGHT_RED,
    CC_COLOR_BRIGHT_MAGENTA, CC_COLOR_BRIGHT_CYAN, CC_COLOR_WHITE,
    CC_COLOR_BRIGHT_WHITE, CC_COLOR_BLACK, CC_COLOR_BRIGHT_BLACK,
};
#define CC_PIE_DEFAULT_PALETTE_COUNT \
    ((int)(sizeof(CC_PIE_DEFAULT_PALETTE) / sizeof(CC_PIE_DEFAULT_PALETTE[0])))

/* Resolves the color for `slice`: the per-slice override palette when set
 * (a NULL-terminated array), otherwise the fixed default palette. */
CC_INLINE const char* cc_pie_palette_color(const cc_pie_settings_t* s, int slice) {
    if (s->colors != NULL) {
        int n = 0;
        while (s->colors[n] != NULL) n++;
        if (n > 0) return s->colors[slice % n];
    }
    return CC_PIE_DEFAULT_PALETTE[slice % CC_PIE_DEFAULT_PALETTE_COUNT];
}

/* Returns the slice index whose angular range contains `angle`, given the
 * slices' start angles. The angle is normalized into the first slice's
 * revolution so the last slice can wrap past +2*pi and back to the first. */
CC_INLINE int cc_pie_find_slice(const double* starts, int count, double angle) {
    double base = starts[0];
    double two_pi = 2.0 * CC_PI;
    while (angle < base) angle += two_pi;
    while (angle >= base + two_pi) angle -= two_pi;
    for (int i = 1; i < count; i++) {
        if (angle < starts[i]) return i - 1;
    }
    return count - 1;
}

/* Gap-aware variant used when slice_gap > 0: each slice spans [starts[i],
 * starts[i] + widths[i]); the region between a slice's end and the next
 * slice's start (and the closing gap before 12 o'clock) is blank, so this
 * returns -1 there. Accounts for the last slice wrapping past the first. */
CC_INLINE int cc_pie_find_slice_gap(const double* starts, const double* widths,
                                    int count, double angle) {
    double base = starts[0];
    double two_pi = 2.0 * CC_PI;
    int i;

    while (angle < base) angle += two_pi;
    while (angle >= base + two_pi) angle -= two_pi;
    for (i = 0; i < count; i++) {
        double a, e;
        if (widths[i] <= 0.0) continue;
        a = starts[i];
        e = a + widths[i];
        if (angle >= a && angle < e) return i;
    }
    return -1;
}

/* Joins the per-cell strings (columns[(x*height + y)*32 .. +32), y counted
 * from the bottom) into the final pie string, printing rows top-to-bottom
 * and appending the legend below when requested. Allocates with a pointer
 * cursor (no strcat re-scans); the caller frees the result. */
CC_INLINE char* cc_pie_assemble(int width, int height, char* columns,
                                const cc_pie_slice_t* slices, int count,
                                double total, const cc_pie_settings_t* s) {
    int legend = s->show_legend ? count : 0;
    /* Bounded by CC_MAX_CELLS for the cells (width*height <= 1e6, each cell
     * at most 32 bytes); each legend row is capped at 512 bytes by the
     * snprintf below, so this allocation is always large enough. */
    size_t total_size = (size_t)width * (size_t)height * 32 + (size_t)height + 1
                        + (size_t)legend * 512;
    char* chart = (char*)calloc(total_size, 1);
    if (chart == NULL) return NULL;

    char* w = chart;
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            const char* cell = columns + ((size_t)x * height + (size_t)y) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }

    if (legend > 0) {
        char line[512];
        for (int i = 0; i < count; i++) {
            const char* label = CC_S(slices[i].label);
            double pct = slices[i].value / total * 100.0;
            switch (s->legend_format) {
            case CC_PIE_LEGEND_LABEL_PCT:
                snprintf(line, sizeof(line), "%s  %.0f%%", label, pct);
                break;
            case CC_PIE_LEGEND_VALUE_PCT:
                snprintf(line, sizeof(line), "%g  (%.0f%%)",
                         slices[i].value, pct);
                break;
            case CC_PIE_LEGEND_LABEL:
                snprintf(line, sizeof(line), "%s", label);
                break;
            case CC_PIE_LEGEND_VALUE:
            default:
                if (s->show_pct) {
                    snprintf(line, sizeof(line), "%s  %g (%.0f%%)", label,
                             slices[i].value, pct);
                } else {
                    snprintf(line, sizeof(line), "%s  %g", label,
                             slices[i].value);
                }
                break;
            }
            size_t ll = strlen(line);
            memcpy(w, line, ll);
            w += ll;
            *w++ = '\n';
        }
    }
    *w = '\0';
    return chart;
}

CC_INLINE char* cc_pie_create(const cc_pie_slice_t* slices, int count,
                              int width, int height,
                              const cc_pie_settings_t* settings) {
    if (slices == NULL || count <= 0 || !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }
    int i;
    double total = 0.0;
    for (i = 0; i < count; i++) {
        /* A plain positive-range check: NaN fails `> 0.0`, as do zero and
         * negative amounts. +inf and non-finite option doubles are rejected
         * upstream (wrapper/ABI), which keeps this header free of C99-only
         * `isfinite`. */
        if (!(slices[i].value > 0.0)) {
            return (char*)calloc(1, sizeof(char));
        }
        total += slices[i].value;
    }

    cc_pie_settings_t s = {0};
    if (settings != NULL) {
        s.bg_color = settings->bg_color;
        s.colors = settings->colors;
        s.donut = settings->donut;
        s.show_legend = settings->show_legend;
        s.show_pct = settings->show_pct;
        s.slice_gap = settings->slice_gap;
        s.inner_radius_ratio = settings->inner_radius_ratio;
        s.legend_format = settings->legend_format;
        s.start_angle = settings->start_angle;
        s.counter_clockwise = settings->counter_clockwise;
        s.center_text = settings->center_text;
    }

    /* Resolve the optional settings to concrete values. Negative doubles are
     * the "unspecified" sentinel: default to the library defaults. */
    if (s.slice_gap < 0.0) s.slice_gap = 0.0;
    if (s.legend_format < CC_PIE_LEGEND_VALUE ||
        s.legend_format > CC_PIE_LEGEND_LABEL) {
        s.legend_format = CC_PIE_LEGEND_VALUE;
    }
    if (s.start_angle < 0.0) s.start_angle = CC_PI / 2.0;

    double* starts = (double*)malloc((size_t)count * sizeof(double));
    double* widths = (s.slice_gap > 0.0)
        ? (double*)malloc((size_t)count * sizeof(double)) : NULL;
    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (starts == NULL || columns == NULL ||
        (s.slice_gap > 0.0 && widths == NULL)) {
        free(starts);
        free(widths);
        free(columns);
        return NULL;
    }

    /* Sub-pixel space: 8 units per cell. The disk is a perfect circle in this
     * space, centered in the grid, with the radius taken from the smaller
     * dimension so it always fits. */
    double cx = (double)width * 4.0;
    double cy = (double)height * 4.0;
    double min_dim = (width < height) ? (double)width : (double)height;
    double outer_r = min_dim * 4.0;
    double inner_r;
    if (s.inner_radius_ratio < 0.0) {
        /* Unspecified: the donut flag's classic hollow (0.5) or a disk. */
        inner_r = s.donut ? outer_r * CC_PIE_DONUT_RADIUS_RATIO : 0.0;
    } else {
        double r = (s.inner_radius_ratio > 1.0) ? 1.0 : s.inner_radius_ratio;
        inner_r = outer_r * r;
    }

    double angle = s.start_angle;
    for (i = 0; i < count; i++) {
        double frac = slices[i].value / total;
        starts[i] = angle;
        if (widths != NULL) {
            double w = frac * 2.0 * CC_PI - s.slice_gap;
            widths[i] = (w < 0.0) ? 0.0 : w;
        }
        angle += frac * 2.0 * CC_PI;
    }

    for (int x = 0; x < width; x++) {
        double sx = (double)x * 8.0 + 4.0;
        for (int y = 0; y < height; y++) {
            double sy = (double)y * 8.0 + 4.0;
            double dx = (sx - cx) * CC_PIE_ASPECT;  /* 1:1 with dy's terminal aspect */
            double dy = sy - cy;
            double dist = sqrt(dx * dx + dy * dy);
            char* cell = columns + ((size_t)x * height + (size_t)y) * 32;

            if (dist > outer_r || (inner_r > 0.0 && dist < inner_r)) {
                if (s.bg_color != NULL) {
                    snprintf(cell, 32, "%s %s", s.bg_color,
                             cc_reset_for(s.bg_color, NULL));
                } else {
                    snprintf(cell, 32, " ");
                }
                continue;
            }

            /* counter_clockwise mirrors the horizontal axis, which reverses
             * the sweep (clockwise) while keeping slice 0 at 12 o'clock. */
            double probe_x = (s.counter_clockwise ? (cx - sx) : (sx - cx))
                             * CC_PIE_ASPECT;
            double probe = atan2(dy, probe_x);
            int slice = (widths != NULL)
                ? cc_pie_find_slice_gap(starts, widths, count, probe)
                : cc_pie_find_slice(starts, count, probe);
            if (slice < 0) {   /* in a slice gap: leave the cell blank */
                if (s.bg_color != NULL) {
                    snprintf(cell, 32, "%s %s", s.bg_color,
                             cc_reset_for(s.bg_color, NULL));
                } else {
                    snprintf(cell, 32, " ");
                }
                continue;
            }
            const char* color = cc_pie_palette_color(&s, slice);
            snprintf(cell, 32, "%s%s%s", CC_S(color), CC_BLOCK_FULL,
                     cc_reset_for(color, NULL));
        }
    }

    /* center_text is drawn in the hollow center, centered on the row nearest
     * the vertical middle and truncated to the hole's width in cells. */
    if (inner_r > 0.0 && s.center_text != NULL && s.center_text[0] != '\0') {
        double best = 1.0e18;
        int y_center = 0;
        for (int y = 0; y < height; y++) {
            double sy = (double)y * 8.0 + 4.0;
            double d = (sy > cy) ? (sy - cy) : (cy - sy);
            if (d < best) {
                best = d;
                y_center = y;
            }
        }
        int x0 = -1;
        int x1 = -1;
        for (int x = 0; x < width; x++) {
            double sx = (double)x * 8.0 + 4.0;
            double d = (sx > cx) ? (sx - cx) : (cx - sx);
            /* Same aspect-scaled hollow test as the disk geometry. */
            if (d * CC_PIE_ASPECT < inner_r) {
                if (x0 < 0) x0 = x;
                x1 = x;
            }
        }
        if (x0 >= 0) {
            size_t tlen = strlen(s.center_text);
            int span = x1 - x0 + 1;
            int n = (span < (int)tlen) ? span : (int)tlen;
            int startcol = x0 + (span - n) / 2;
            for (int k = 0; k < n; k++) {
                char* cell = columns +
                             ((size_t)(startcol + k) * height +
                              (size_t)y_center) * 32;
                snprintf(cell, 32, "%c", s.center_text[k]);
            }
        }
    }

    char* chart = cc_pie_assemble(width, height, columns, slices, count,
                                  total, &s);
    free(starts);
    free(widths);
    free(columns);
    return chart;
}

/* ---------------------------- Histogram renderer ---------------------------
 * Samples are counted into equal-width bins across a [min, max] value
 * window; each bin / column is drawn as a vertical bar scaled to the
 * tallest bin and sampled at 8 sub-pixels per cell (cc_lower_eighth), so
 * bar tops are smooth like the line chart. When width >= bin_count a bin
 * may span several columns; when width < bin_count several bins are
 * aggregated into each column — a single deterministic long-arithmetic
 * mapping covers both. Outlier samples are clamped into the edge bins so a
 * single huge value cannot empty every other bin, and a flat window
 * (min == max) is widened by +-1 so the value-to-bin division never hits
 * zero. Non-finite samples are rejected upstream (wrapper/ABI); a NaN
 * window double is the "auto" sentinel that selects the data range. All
 * scratch buffers are heap-allocated (no VLAs) and freed on every path.
 * ------------------------------------------------------------------------- */

/* Joins the per-cell strings (columns[(x*height + y)*32 .. +32), y counted
 * from the bottom) into the final histogram string, printing rows
 * top-to-bottom. With show_prices it prepends an 8-column left margin
 * holding the max-count (top) / min-count (bottom) labels; with show_bins
 * it appends a value-axis footer row (window min left, window max right).
 * Allocates with a pointer cursor (no strcat re-scans); the caller frees
 * the result. */
CC_INLINE char* cc_hist_assemble(int width, int height, char* columns,
                                 const cc_hist_settings_t* s,
                                 double wmin, double wmax,
                                 int max_count, int min_count) {
    int margin = s->show_prices ? 8 : 0;
    char max_label[16] = "";
    char min_label[16] = "";
    if (margin > 0) {
        snprintf(max_label, sizeof(max_label), "%8d", max_count);
        snprintf(min_label, sizeof(min_label), "%8d", min_count);
    }

    char lo[16] = "";
    char hi[16] = "";
    int footer = 0;
    if (s->show_bins) {
        snprintf(lo, sizeof(lo), "%.2f", wmin);
        snprintf(hi, sizeof(hi), "%.2f", wmax);
        footer = 1;
    }

    const int line_len = width + margin;
    const int rows = height + footer;
    /* Bounded by CC_MAX_CELLS: width*height <= 1e6, so this is <= ~32 MB. */
    size_t total_size = (size_t)line_len * (size_t)rows * 32 + (size_t)rows + 1;
    char* chart = (char*)calloc(total_size, 1);
    if (chart == NULL) return NULL;

    char* w = chart;
    for (int y = height - 1; y >= 0; y--) {
        if (margin > 0) {
            char mlabel[16] = "";
            if (y == height - 1) snprintf(mlabel, sizeof(mlabel), "%8s", max_label);
            else if (y == 0)     snprintf(mlabel, sizeof(mlabel), "%8s", min_label);
            else { memset(mlabel, ' ', 8); mlabel[8] = '\0'; }
            size_t ml = strlen(mlabel);
            memcpy(w, mlabel, ml);
            w += ml;
        }
        for (int x = 0; x < width; x++) {
            const char* cell = columns + ((size_t)x * height + (size_t)y) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }

    if (footer) {
        for (int i = 0; i < line_len; i++) *w++ = ' ';
        size_t l0 = strlen(lo);
        if (l0 > (size_t)line_len) l0 = (size_t)line_len;
        size_t l1 = strlen(hi);
        if (l1 > (size_t)line_len) l1 = (size_t)line_len;
        if (l0 > 0) memcpy(w - line_len, lo, l0);
        if (l1 > 0) memcpy(w - l1, hi, l1);
        *w++ = '\n';
    }
    *w = '\0';
    return chart;
}

CC_INLINE char* cc_hist_create(const double* samples, int count,
                               int width, int height,
                               const cc_hist_settings_t* settings) {
    if (samples == NULL || count <= 0 || !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }

    cc_hist_settings_t s;
    s.rise_color  = (settings && settings->rise_color) ? settings->rise_color : CC_COLOR_GREEN;
    s.bg_color    = settings ? settings->bg_color : NULL;
    s.bin_count   = settings ? settings->bin_count : 0;
    /* NaN is the "auto" sentinel for the window doubles (0.0 is meaningful). */
    s.min_value   = (settings && settings->min_value == settings->min_value)
                    ? settings->min_value : CC_NAN;
    s.max_value   = (settings && settings->max_value == settings->max_value)
                    ? settings->max_value : CC_NAN;
    s.show_bins   = settings ? settings->show_bins : 0;
    s.show_prices = settings ? settings->show_prices : 0;

    double dmin = samples[0];
    double dmax = samples[0];
    for (int i = 1; i < count; i++) {
        if (samples[i] < dmin) dmin = samples[i];
        if (samples[i] > dmax) dmax = samples[i];
    }

    /* The value window used for binning. A valid explicit window ALWAYS
     * has min < max; anything else — the NaN auto-sentinels, a partial
     * {0} initializer that yields 0.0/0.0, or an inverted range — falls
     * back to the data min/max so a brace-init {0} histogram auto-scales
     * and renders multiple bars instead of fixing a single-bar [0,0] window. */
    double wmin, wmax;
    if (s.min_value < s.max_value) {
        wmin = s.min_value;
        wmax = s.max_value;
    } else {
        wmin = dmin;
        wmax = dmax;
        if (wmax == wmin) {   /* all samples equal: widen the flat window */
            wmin -= 1.0;
            wmax += 1.0;
        }
    }
    double range = wmax - wmin;

    /* Auto-select the bin count when <= 0: 20 for busy sequences, 10
     * otherwise, and never more bins than the chart has columns. */
    int bins = s.bin_count;
    if (bins <= 0) {
        bins = (count >= 40) ? 20 : 10;
    }
    if (bins > width) bins = width;
    if (bins < 1) bins = 1;

    int* counts = (int*)calloc((size_t)bins, sizeof(int));
    int* colv = (int*)calloc((size_t)width, sizeof(int));
    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (counts == NULL || colv == NULL || columns == NULL) {
        free(counts); free(colv); free(columns);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        double t = (samples[i] - wmin) / range;
        int b = (int)(t * bins);
        if (b < 0) b = 0;
        if (b >= bins) b = bins - 1;
        counts[b]++;
    }

    /* Fold the bins into `width` columns. The same long-arithmetic mapping
     * covers both decompositions: width >= bins gives each bin several
     * columns (aggregating the single bin's count), width < bins aggregates
     * several bins per column. */
    for (int w = 0; w < width; w++) {
        int bs = (int)(((long long)w * bins) / width);
        int be = (int)((((long long)w + 1) * bins) / width);
        if (be <= bs) be = bs + 1;
        int sum = 0;
        for (int b = bs; b < be && b < bins; b++) sum += counts[b];
        colv[w] = sum;
    }

    int max_count = 0;
    int min_count = colv[0];
    for (int w = 0; w < width; w++) {
        if (colv[w] > max_count) max_count = colv[w];
        if (colv[w] < min_count) min_count = colv[w];
    }

    const char* color = s.rise_color;
    int pixel_height = height * 8;

    for (int w = 0; w < width; w++) {
        int pix;
        if (max_count <= 0) pix = 0;
        else pix = (int)((double)colv[w] * pixel_height / max_count);
        if (pix < 0) pix = 0;
        if (pix > pixel_height) pix = pixel_height;

        int nfull = pix / 8;
        int rem = pix % 8;
        char* col = columns + (size_t)w * height * 32;

        for (int r = 0; r < nfull && r < height; r++) {
            snprintf(col + (size_t)r * 32, 32, "%s%s%s", CC_S(color),
                     CC_BLOCK_FULL, cc_reset_for(color, NULL));
        }
        if (rem > 0 && nfull < height) {
            snprintf(col + (size_t)nfull * 32, 32, "%s%s%s", CC_S(color),
                     cc_lower_eighth(rem), cc_reset_for(color, NULL));
        }
        for (int r = (rem > 0 ? nfull + 1 : nfull); r < height; r++) {
            char* cell = col + (size_t)r * 32;
            if (s.bg_color != NULL) {
                snprintf(cell, 32, "%s %s", s.bg_color,
                         cc_reset_for(s.bg_color, NULL));
            } else {
                snprintf(cell, 32, " ");
            }
        }
    }

    char* chart = cc_hist_assemble(width, height, columns, &s,
                                   wmin, wmax, max_count, min_count);
    free(counts); free(colv); free(columns);
    return chart;
}

/* ---------------------------- Sparkline renderer ---------------------------
 * An axis-less trend line. For each output column, average the samples that
 * fall into that column (exactly the line chart's downsampling mapping, so
 * width < count aggregates and width >= count spreads), map the column
 * average to an 8-sub-pixel row, then draw the column exactly like the line
 * chart: cells below the (current,next) line segment get the optional area
 * fill (or blank), the top partial cell uses cc_lower_eighth, and rows are
 * assembled top-to-bottom with no margin, footer or legend. min_above /
 * min_below reserve sub-pixels at the plot's top/bottom edges so the line
 * does not clip against the very edge at tiny heights. A flat (all-equal)
 * series is centered (cc_pixel semantics: range 0 -> middle), so there is
 * no divide-by-zero. Non-finite samples are rejected upstream (wrapper/ABI).
 * All scratch buffers are heap-allocated (no VLAs), sizeof(char) calloc on
 * bad args, and freed on every path. ------------------------------------------------------------------------- */

/* Joins the per-cell strings (columns[(x*height + y)*32 .. +32), y counted
 * from the bottom) into the final sparkline string, printing rows
 * top-to-bottom with a trailing newline per row and no margin/footer/legend.
 * Allocates with a pointer cursor (no strcat re-scans); caller frees. */
CC_INLINE char* cc_spark_assemble(int width, int height, char* columns) {
    /* Bounded by CC_MAX_CELLS: width*height <= 1e6, so this is <= ~32 MB. */
    size_t total_size = (size_t)width * (size_t)height * 32 + (size_t)height + 1;
    char* chart = (char*)calloc(total_size, 1);
    if (chart == NULL) return NULL;

    char* w = chart;
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            const char* cell = columns + ((size_t)x * height + (size_t)y) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }
    *w = '\0';
    return chart;
}

CC_INLINE char* cc_spark_create(const double* samples, int count,
                                int width, int height,
                                const cc_spark_settings_t* settings) {
    if (samples == NULL || count <= 0 || !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }

    cc_spark_settings_t s;
    /* min_above/min_below are plain ints: a partial {0} brace initializer or
     * a NULL settings both yield 0, so no sentinel ambiguity (unlike the
     * histogram's double window fields). */
    s.rise_color = (settings && settings->rise_color) ? settings->rise_color : CC_COLOR_GREEN;
    s.area_color = (settings && settings->area_color) ? settings->area_color : NULL;
    s.min_above  = settings ? settings->min_above : 0;
    s.min_below  = settings ? settings->min_below : 0;
    if (s.min_above < 0) s.min_above = 0;
    if (s.min_below < 0) s.min_below = 0;

    double* vals = (double*)calloc((size_t)width, sizeof(double));
    int* py = (int*)malloc((size_t)width * sizeof(int));
    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (vals == NULL || py == NULL || columns == NULL) {
        free(vals); free(py); free(columns);
        return NULL;
    }

    /* Column averages, exactly the line chart's downsampling mapping. */
    for (int w = 0; w < width; w++) {
        int start_idx = (int)(((long long)w * count) / width);
        int end_idx = (int)((((long long)w + 1) * count) / width);
        if (end_idx <= start_idx) end_idx = start_idx + 1;

        double sum = 0.0;
        for (int k = start_idx; k < end_idx && k < count; k++) {
            sum += samples[k];
        }
        vals[w] = sum / (end_idx - start_idx);
    }

    double min = find_min(vals, width);
    double max = find_max(vals, width);
    double range = max - min;

    int pixel_height = height * 8;
    int span = pixel_height - s.min_above - s.min_below;
    if (span < 1) span = 1;

    for (int i = 0; i < width; i++) {
        /* Mirrors cc_pixel but reserves the top/bottom sub-pixel margins.
         * range == 0 (flat series) centers the line in the usable band. */
        double t = (range == 0.0) ? 0.5 : (vals[i] - min) / range;
        int p = (int)lround(s.min_above + t * (span - 1));
        if (p < 0) p = 0;
        if (p >= pixel_height) p = pixel_height - 1;
        py[i] = p;
    }

    for (int i = 0; i < width; i++) {
        int lo = py[i];
        int hi = py[i];
        if (i + 1 < width) {
            if (py[i+1] < lo) lo = py[i+1];
            if (py[i+1] > hi) hi = py[i+1];
        }

        int cell_lo = lo / 8;
        int cell_hi = hi / 8;
        char* col = columns + (size_t)i * height * 32;

        /* Below the line segment: area fill or blank. */
        for (int r = 0; r < cell_lo && r < height; r++) {
            char* cell = col + (size_t)r * 32;
            if (s.area_color != NULL) {
                snprintf(cell, 32, "%s %s", s.area_color,
                         cc_reset_for(s.area_color, NULL));
            } else {
                snprintf(cell, 32, " ");
            }
        }

        /* Line body: full blocks between columns' pixels. */
        for (int r = cell_lo; r < cell_hi && r < height; r++) {
            snprintf(col + (size_t)r * 32, 32, "%s%s%s", CC_S(s.rise_color),
                     CC_BLOCK_FULL, cc_reset_for(s.rise_color, NULL));
        }

        /* Top partial cell: smooth 8-level top. */
        if (cell_hi >= 0 && cell_hi < height) {
            int count8 = hi - cell_hi * 8 + 1;
            if (count8 > 8) count8 = 8;
            snprintf(col + (size_t)cell_hi * 32, 32, "%s%s%s",
                     CC_S(s.rise_color), cc_lower_eighth(count8),
                     cc_reset_for(s.rise_color, NULL));
        }

        /* Above the line: blank. */
        for (int r = cell_hi + 1; r < height; r++) {
            snprintf(col + (size_t)r * 32, 32, " ");
        }
    }

    char* chart = cc_spark_assemble(width, height, columns);
    free(vals); free(py); free(columns);
    return chart;
}

/* ------------------------------ Bar renderer ------------------------------
 * An ordered list of (label, value) pairs drawn as vertical bars from a
 * shared zero baseline. Each output column maps to a deterministic span of
 * items via the same long-arithmetic mapping as the histogram and sparkline
 * (width >= count spreads an item over several columns; width < count folds
 * several items into one column, drawn at their max value). The tallest bar
 * fills the full height; heights use 8 sub-pixel rows (cc_lower_eighth tops)
 * like the line chart. Values are non-negative for now — a negative value is
 * clamped to zero — and non-finite values are rejected upstream (wrapper/ABI)
 * so this header stays free of C99-only `isfinite`. Scratch buffers are
 * heap-allocated (no VLAs), sizeof(char) calloc on bad args, freed on every
 * path. ------------------------------------------------------------------- */

/* Joins the per-cell strings (columns[(x*height + y)*32 .. +32), y counted
 * from the bottom) into the final bar string, printing rows top-to-bottom.
 * An optional 8-column value axis (show_prices: max value top, 0 bottom) is
 * prepended to every row, and an optional label footer (show_labels) is
 * appended below the plot: for each column, the label of the first item in
 * that column's span, truncated to the column width. Allocates with a pointer
 * cursor (no strcat re-scans); caller frees. */
CC_INLINE char* cc_bar_assemble(int width, int height, char* columns,
                                const cc_bar_item_t* items, int count,
                                const cc_bar_settings_t* s,
                                double max_value,
                                const int* first_idx, const int* span_len) {
    int margin = s->show_prices ? 8 : 0;
    char max_label[16] = "";
    char min_label[16] = "";
    if (margin > 0) {
        snprintf(max_label, sizeof(max_label), "%8.4g", max_value);
        snprintf(min_label, sizeof(min_label), "%8.0f", 0.0);
    }

    int footer = s->show_labels ? 1 : 0;
    const int line_len = width + margin;
    const int rows = height + footer;
    /* Bounded by CC_MAX_CELLS: width*height <= 1e6, so this is <= ~32 MB. */
    size_t total_size = (size_t)line_len * (size_t)rows * 32 + (size_t)rows + 1;
    char* chart = (char*)calloc(total_size, 1);
    if (chart == NULL) return NULL;

    char* w = chart;
    for (int y = height - 1; y >= 0; y--) {
        if (margin > 0) {
            char mlabel[16] = "";
            if (y == height - 1) snprintf(mlabel, sizeof(mlabel), "%8s", max_label);
            else if (y == 0)     snprintf(mlabel, sizeof(mlabel), "%8s", min_label);
            else { memset(mlabel, ' ', 8); mlabel[8] = '\0'; }
            size_t ml = strlen(mlabel);
            memcpy(w, mlabel, ml);
            w += ml;
        }
        for (int x = 0; x < width; x++) {
            const char* cell = columns + ((size_t)x * height + (size_t)y) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }

    if (footer) {
        if (margin > 0) {
            memset(w, ' ', (size_t)margin);
            w += margin;
        }
        for (int x = 0; x < width; x++) {
            int span = span_len[x];
            if (span < 1) span = 1;
            const char* label = items[first_idx[x]].label;
            const char* L = (label != NULL) ? label : "";
            size_t n = strlen(L);
            if (n > (size_t)span) n = (size_t)span;
            if (n > 0) { memcpy(w, L, n); w += n; }
            if ((size_t)span > n) {
                memset(w, ' ', (size_t)span - n);
                w += (size_t)span - n;
            }
        }
        *w++ = '\n';
    }
    *w = '\0';
    (void)count;
    return chart;
}

CC_INLINE char* cc_bar_create(const cc_bar_item_t* items, int count,
                              int width, int height,
                              const cc_bar_settings_t* settings) {
    if (items == NULL || count <= 0 || !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }

    cc_bar_settings_t s;
    /* All pointer/int fields: a partial {0} brace initializer matches NULL,
     * so there is no sentinel ambiguity (unlike the histogram's doubles). */
    s.rise_color = (settings && settings->rise_color) ? settings->rise_color : CC_COLOR_GREEN;
    s.bg_color   = settings ? settings->bg_color : NULL;
    s.show_labels = settings ? settings->show_labels : 0;
    s.show_prices = settings ? settings->show_prices : 0;
    if (s.show_labels < 0) s.show_labels = 0;
    if (s.show_prices < 0) s.show_prices = 0;

    double* colv = (double*)calloc((size_t)width, sizeof(double));
    int* first_idx = (int*)malloc((size_t)width * sizeof(int));
    int* span_len = (int*)malloc((size_t)width * sizeof(int));
    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (colv == NULL || first_idx == NULL || span_len == NULL || columns == NULL) {
        free(colv); free(first_idx); free(span_len); free(columns);
        return NULL;
    }

    /* Fold the items into `width` columns. The same long-arithmetic mapping
     * covers both decompositions: width >= count spreads each item over
     * several columns (repeating its value), width < count aggregates several
     * items per column drawn at their max value (so a peak is never hidden by
     * a collapsing average). */
    double max_value = 0.0;
    for (int w = 0; w < width; w++) {
        int is = (int)(((long long)w * count) / width);
        int ie = (int)(((((long long)w + 1) * count) / width));
        if (ie <= is) ie = is + 1;
        if (ie > count) ie = count;
        first_idx[w] = is;
        span_len[w] = ie - is;

        double v = 0.0;   /* zero baseline; negative values clamp to 0 */
        for (int k = is; k < ie; k++) {
            double val = items[k].value;
            if (val < 0.0) val = 0.0;
            if (val > v) v = val;
        }
        colv[w] = v;
        if (v > max_value) max_value = v;
    }

    const char* color = s.rise_color;
    int pixel_height = height * 8;

    for (int w = 0; w < width; w++) {
        int pix;
        if (max_value <= 0.0) pix = 0;
        else pix = (int)(colv[w] * pixel_height / max_value);
        if (pix < 0) pix = 0;
        if (pix > pixel_height) pix = pixel_height;

        int nfull = pix / 8;
        int rem = pix % 8;
        char* col = columns + (size_t)w * height * 32;

        for (int r = 0; r < nfull && r < height; r++) {
            snprintf(col + (size_t)r * 32, 32, "%s%s%s", CC_S(color),
                     CC_BLOCK_FULL, cc_reset_for(color, NULL));
        }
        if (rem > 0 && nfull < height) {
            snprintf(col + (size_t)nfull * 32, 32, "%s%s%s", CC_S(color),
                     cc_lower_eighth(rem), cc_reset_for(color, NULL));
        }
        for (int r = (rem > 0 ? nfull + 1 : nfull); r < height; r++) {
            char* cell = col + (size_t)r * 32;
            if (s.bg_color != NULL) {
                snprintf(cell, 32, "%s %s", s.bg_color,
                         cc_reset_for(s.bg_color, NULL));
            } else {
                snprintf(cell, 32, " ");
            }
        }
    }

    char* chart = cc_bar_assemble(width, height, columns, items, count, &s,
                                  max_value, first_idx, span_len);
    free(colv); free(first_idx); free(span_len); free(columns);
    return chart;
}

/* --------------------------- Stacked bar renderer -------------------------
 * A stacked bar is a vertical stack of segments (one per series) per
 * category; the category's bar height is the SUM of its series' values. All
 * series share one category count (settings->cats). Within a column the
 * segments are stacked bottom-to-top, proportional to each series'
 * value / the tallest stack total, drawn with 8 sub-pixel rows exactly like
 * the bar chart: every cell is colored by the series that sits on the stack's
 * surface at that cell, and the stack's top partial cell uses cc_lower_eighth
 * (so the final, tallest series' cell carries the top edge). Falls back to
 * the pie default palette or a per-series override. All scratch buffers are
 * heap-allocated (no VLAs) and freed on every path. */

CC_INLINE const char* cc_stack_palette_color(const cc_stack_settings_t* s,
                                             int series_idx) {
    if (s->colors != NULL) {
        int n = 0;
        while (s->colors[n] != NULL) n++;
        if (n > 0) return s->colors[series_idx % n];
    }
    return CC_PIE_DEFAULT_PALETTE[series_idx % CC_PIE_DEFAULT_PALETTE_COUNT];
}

/* Assembles the plot columns plus optional value-axis margin and category
 * label footer into the returned malloc'd string (caller frees). `max_total`
 * is the tallest stack total for the value axis; `cat_labels` (may be NULL)
 * supplies each column's footer label from the first category in that
 * column's span, truncated to the column width like the bar footer. Allocates
 * with a pointer cursor; caller frees. */
CC_INLINE char* cc_stack_assemble(int width, int height, char* columns,
                                  const cc_stack_settings_t* s,
                                  double max_total,
                                  const char* const* cat_labels,
                                  const int* first_idx, const int* span_len) {
    int margin = s->show_prices ? 8 : 0;
    char max_label[16] = "";
    char min_label[16] = "";
    if (margin > 0) {
        snprintf(max_label, sizeof(max_label), "%8.4g", max_total);
        snprintf(min_label, sizeof(min_label), "%8.0f", 0.0);
    }

    int footer = s->show_labels ? 1 : 0;
    const int line_len = width + margin;
    const int rows = height + footer;
    /* Bounded by CC_MAX_CELLS: width*height <= 1e6, so this is <= ~32 MB. */
    size_t total_size = (size_t)line_len * (size_t)rows * 32 + (size_t)rows + 1;
    char* chart = (char*)calloc(total_size, 1);
    if (chart == NULL) return NULL;

    char* w = chart;
    for (int y = height - 1; y >= 0; y--) {
        if (margin > 0) {
            char mlabel[16] = "";
            if (y == height - 1) snprintf(mlabel, sizeof(mlabel), "%8s", max_label);
            else if (y == 0)     snprintf(mlabel, sizeof(mlabel), "%8s", min_label);
            else { memset(mlabel, ' ', 8); mlabel[8] = '\0'; }
            size_t ml = strlen(mlabel);
            memcpy(w, mlabel, ml);
            w += ml;
        }
        for (int x = 0; x < width; x++) {
            const char* cell = columns + ((size_t)x * height + (size_t)y) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }

    if (footer) {
        if (margin > 0) {
            memset(w, ' ', (size_t)margin);
            w += margin;
        }
        for (int x = 0; x < width; x++) {
            int span = span_len[x];
            if (span < 1) span = 1;
            const char* label = (cat_labels != NULL) ? cat_labels[first_idx[x]] : NULL;
            const char* L = (label != NULL) ? label : "";
            size_t n = strlen(L);
            if (n > (size_t)span) n = (size_t)span;
            if (n > 0) { memcpy(w, L, n); w += n; }
            if ((size_t)span > n) {
                memset(w, ' ', (size_t)span - n);
                w += (size_t)span - n;
            }
        }
        *w++ = '\n';
    }
    *w = '\0';
    return chart;
}

CC_INLINE char* cc_stack_create(const cc_stack_series_t* series, int series_count,
                                int width, int height,
                                const cc_stack_settings_t* settings) {
    /* The values lengths come only from settings->cats, so a NULL settings
     * (or cats <= 0) is a parsing failure, not a "use defaults" signal. */
    if (series == NULL || series_count <= 0 || !cc_dim_ok(width, height) ||
        settings == NULL || settings->cats <= 0) {
        return (char*)calloc(1, sizeof(char));
    }

    int cats = settings->cats;

    cc_stack_settings_t s;
    /* All pointer/int fields: a partial {0} brace initializer matches NULL,
     * so there is no sentinel ambiguity (unlike the histogram's doubles). */
    s.colors      = settings->colors;
    s.bg_color    = settings->bg_color;
    s.cat_labels  = settings->cat_labels;
    s.series      = settings->series;
    s.cats        = cats;
    s.show_labels = (settings->show_labels < 0) ? 0 : settings->show_labels;
    s.show_prices = (settings->show_prices < 0) ? 0 : settings->show_prices;

    double* segs = (double*)malloc((size_t)series_count * sizeof(double));
    double* cum = (double*)malloc(((size_t)series_count + 1) * sizeof(double));
    int* pix = (int*)malloc(((size_t)series_count + 1) * sizeof(int));
    double* coltot = (double*)malloc((size_t)width * sizeof(double));
    int* first_idx = (int*)malloc((size_t)width * sizeof(int));
    int* span_len = (int*)malloc((size_t)width * sizeof(int));
    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (segs == NULL || cum == NULL || pix == NULL || coltot == NULL ||
        first_idx == NULL || span_len == NULL || columns == NULL) {
        free(segs); free(cum); free(pix); free(coltot);
        free(first_idx); free(span_len); free(columns);
        return NULL;
    }

    /* Fold the categories into `width` columns. The same long-arithmetic
     * mapping covers both decompositions: width >= cats spreads each category
     * over several columns (repeating its totals), width < cats aggregates
     * several categories per column whose segment sums are ADDED (stacked
     * bars are about totals). */
    double max_total = 0.0;
    for (int w = 0; w < width; w++) {
        int is = (int)(((long long)w * cats) / width);
        int ie = (int)(((((long long)w + 1) * cats) / width));
        if (ie <= is) ie = is + 1;
        if (ie > cats) ie = cats;
        first_idx[w] = is;
        span_len[w] = ie - is;

        double ct = 0.0;   /* zero baseline; negative values clamp to 0 */
        for (int s_i = 0; s_i < series_count; s_i++) {
            const double* vv = series[s_i].values;
            double seg = 0.0;
            for (int c = is; c < ie; c++) {
                double val = vv[c];
                if (val < 0.0) val = 0.0;
                seg += val;
            }
            segs[s_i] = seg;
            ct += seg;
        }
        coltot[w] = ct;
        if (ct > max_total) max_total = ct;
    }

    int pixel_height = height * 8;
    double scale = (max_total > 0.0) ? (double)pixel_height / max_total : 0.0;

    for (int w = 0; w < width; w++) {
        /* Running top pixel after each series (cum[s+1]) — the boundary
         * between stacked segment s and s+1 in render pixels. */
        cum[0] = 0.0;
        for (int s_i = 0; s_i < series_count; s_i++) cum[s_i + 1] = cum[s_i] + segs[s_i];
        cum[series_count] = coltot[w];   /* exact, not a rounding drift */
        for (int s_i = 0; s_i <= series_count; s_i++) {
            pix[s_i] = (int)(cum[s_i] * scale);
            if (pix[s_i] < 0) pix[s_i] = 0;
            if (pix[s_i] > pixel_height) pix[s_i] = pixel_height;
        }
        int stack_top = pix[series_count];

        char* col = columns + (size_t)w * height * 32;
        for (int r = 0; r < height; r++) {
            char* cell = col + (size_t)r * 32;
            int cell_lo = r * 8;
            if (stack_top <= cell_lo) {
                if (s.bg_color != NULL) {
                    snprintf(cell, 32, "%s %s", s.bg_color,
                             cc_reset_for(s.bg_color, NULL));
                } else {
                    snprintf(cell, 32, " ");
                }
                continue;
            }
            /* The surface series: the segment that owns the topmost filled
             * pixel in this cell. pix is the running top after each series,
             * so the owning segment s satisfies pix[s] <= top_pixel <
             * pix[s+1]; zero-height (collapsed) series are skipped by the
             * loop naturally. */
            int cell_hi = r * 8 + 8;
            int top_pixel = (stack_top < cell_hi) ? (stack_top - 1) : (cell_hi - 1);
            int top_s = 0;
            while (top_s < series_count && pix[top_s + 1] <= top_pixel) top_s++;
            const char* color = cc_stack_palette_color(&s, top_s);
            int filled = stack_top - cell_lo;
            if (filled >= 8) {
                snprintf(cell, 32, "%s%s%s", CC_S(color), CC_BLOCK_FULL,
                         cc_reset_for(color, NULL));
            } else {
                /* 1..7: the stack's top partial cell, in the surface series. */
                snprintf(cell, 32, "%s%s%s", CC_S(color), cc_lower_eighth(filled),
                         cc_reset_for(color, NULL));
            }
        }
    }

    char* chart = cc_stack_assemble(width, height, columns, &s, max_total,
                                    s.cat_labels, first_idx, span_len);
    free(segs); free(cum); free(pix); free(coltot);
    free(first_idx); free(span_len); free(columns);
    return chart;
}

/* ------------------------------ Heatmap renderer --------------------------
 * A heatmap draws a 2-D matrix onto a grid, coloring every covered cell by
 * its value's position between the matrix min and max on the fixed ladder
 * (CC_HEAT_RAMP above). Grid row 0 is the TOP (matrix row 0); columns run
 * left to right (matrix column 0). Each covered cell block-averages the
 * matrix values in its span; cells the matrix does not reach (matrix smaller
 * than the grid) are background. All scratch buffers are heap-allocated (no
 * VLAs) and freed on every path. */

/* Returns the matrix row span [*ys, *ye) that output row `gy` (from the top)
 * covers, plus a non-zero `*covered` when that span is non-empty. When the
 * matrix has rows > height it is downsampled (block-average); otherwise each
 * matrix row occupies its own output row on top and the rest are background. */
static void cc_heat_row_span(int rows, int height, int gy,
                             int* ys, int* ye, int* covered) {
    if (rows <= height) {
        if (gy < rows) { *ys = gy; *ye = gy + 1; *covered = 1; }
        else           { *ys = rows; *ye = rows; *covered = 0; }
    } else {
        *ys = (int)(((long long)gy * rows) / height);
        *ye = (int)(((long long)(gy + 1) * rows) / height);
        if (*ye <= *ys) *ye = *ys + 1;
        if (*ye > rows) *ye = rows;
        *covered = (*ys < rows) ? 1 : 0;
    }
}

static void cc_heat_col_span(int cols, int width, int gx,
                             int* xs, int* xe, int* covered) {
    if (cols <= width) {
        if (gx < cols) { *xs = gx; *xe = gx + 1; *covered = 1; }
        else           { *xs = cols; *xe = cols; *covered = 0; }
    } else {
        *xs = (int)(((long long)gx * cols) / width);
        *xe = (int)(((long long)(gx + 1) * cols) / width);
        if (*xe <= *xs) *xe = *xs + 1;
        if (*xe > cols) *xe = cols;
        *covered = (*xs < cols) ? 1 : 0;
    }
}

CC_INLINE char* cc_heat_create(const double* values, int rows, int cols,
                               int width, int height,
                               const cc_heat_settings_t* settings) {
    int gy, gx, i;
    if (values == NULL || rows <= 0 || cols <= 0 ||
        !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }

    cc_heat_settings_t s;
    /* All pointer/int fields: a partial {0} brace initializer matches NULL,
     * so there is no sentinel ambiguity (unlike the histogram's doubles). */
    s.low_color   = (settings && settings->low_color)  ? settings->low_color  : CC_COLOR_BRIGHT_BLACK;
    s.high_color  = (settings && settings->high_color) ? settings->high_color : CC_COLOR_BRIGHT_WHITE;
    s.mid_color   = settings ? settings->mid_color : NULL;
    s.bg_color    = settings ? settings->bg_color  : NULL;
    s.row_labels  = settings ? settings->row_labels  : NULL;
    s.col_labels  = settings ? settings->col_labels  : NULL;
    s.show_labels = (settings && settings->show_labels < 0) ? 0
                    : (settings ? settings->show_labels : 0);

    double dmin = values[0];
    double dmax = values[0];
    for (i = 1; i < rows * cols; i++) {
        if (values[i] < dmin) dmin = values[i];
        if (values[i] > dmax) dmax = values[i];
    }
    double range = dmax - dmin;

    /* Resolve the ladder: fixed ramp, with the settings color endpoints
     * substituted and an optional mid color replacing the middle entry. */
    const char* ladder[CC_HEAT_RAMP_LEN];
    ladder[0] = CC_COLOR_BRIGHT_BLACK;
    ladder[1] = CC_COLOR_BLUE;
    ladder[2] = CC_COLOR_CYAN;
    ladder[3] = CC_COLOR_BRIGHT_CYAN;
    ladder[4] = CC_COLOR_GREEN;
    ladder[5] = CC_COLOR_YELLOW;
    ladder[6] = CC_COLOR_BRIGHT_YELLOW;
    ladder[7] = CC_COLOR_RED;
    ladder[8] = CC_COLOR_BRIGHT_RED;
    ladder[9] = CC_COLOR_BRIGHT_WHITE;
    ladder[0] = s.low_color;
    ladder[CC_HEAT_RAMP_LEN - 1] = s.high_color;
    if (s.mid_color != NULL) ladder[CC_HEAT_MID_INDEX] = s.mid_color;
    /* Plain mode mirrors the other charts' empty-color convention: an empty
     * low_color (what the Python plain=True path hands in) blanks the WHOLE
     * ladder so no ANSI escape from the fixed interior ramp leaks out. */
    if (s.low_color != NULL && s.low_color[0] == '\0') {
        for (i = 0; i < CC_HEAT_RAMP_LEN; i++) ladder[i] = "";
    }

    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (columns == NULL) return NULL;

    /* Cells are indexed [(gy * width + gx) * 32], gy from the TOP, so the
     * assembly loop reads rows top-to-bottom in matrix order. */
    for (gy = 0; gy < height; gy++) {
        int ys, ye, ycov;
        cc_heat_row_span(rows, height, gy, &ys, &ye, &ycov);
        for (gx = 0; gx < width; gx++) {
            int xs, xe, xcov;
            cc_heat_col_span(cols, width, gx, &xs, &xe, &xcov);
            char* cell = columns + ((size_t)gy * (size_t)width + (size_t)gx) * 32;

            if (!ycov || !xcov) {
                if (s.bg_color != NULL) {
                    snprintf(cell, 32, "%s %s", s.bg_color,
                             cc_reset_for(s.bg_color, NULL));
                } else {
                    snprintf(cell, 32, " ");
                }
                continue;
            }

            /* Block-average the covered matrix cells. */
            double sum = 0.0;
            int r, c, n = 0;
            for (r = ys; r < ye; r++) {
                for (c = xs; c < xe; c++) {
                    sum += values[(size_t)r * (size_t)cols + (size_t)c];
                    n++;
                }
            }
            if (n < 1) continue;
            double avg = sum / (double)n;

            double rr = (range > 0.0) ? (avg - dmin) / range : 0.0;
            if (rr < 0.0) rr = 0.0;
            if (rr > 1.0) rr = 1.0;
            int idx = (int)(rr * (CC_HEAT_RAMP_LEN - 1));
            if (idx >= CC_HEAT_RAMP_LEN) idx = CC_HEAT_RAMP_LEN - 1;
            const char* color = ladder[idx];
            snprintf(cell, 32, "%s%s%s", CC_S(color), CC_BLOCK_FULL,
                     cc_reset_for(color, NULL));
        }
    }

    /* Label frame: an optional left row-label margin and an optional footer
     * row of column labels, both gated by show_labels, mirroring the bar/
     * stack footer (each label truncated to its column span). */
    int rmargin = 0;
    if (s.show_labels && s.row_labels != NULL) {
        for (i = 0; i < rows; i++) {
            if (s.row_labels[i] != NULL) {
                size_t n = strlen(s.row_labels[i]);
                if (n > (size_t)rmargin) rmargin = (int)n;
            }
        }
        if (rmargin > 255) rmargin = 255;   /* fixed label-margin cap */
    }
    int footer = (s.show_labels && s.col_labels != NULL) ? 1 : 0;
    const int line_len = width + rmargin;
    const int rows_out = height + footer;
    /* Bounded by CC_MAX_CELLS: width*height <= 1e6. */
    size_t total = (size_t)line_len * (size_t)rows_out * 32 + (size_t)rows_out + 1;
    char* chart = (char*)calloc(total, 1);
    if (chart == NULL) {
        free(columns);
        return NULL;
    }

    char* w = chart;
    for (gy = 0; gy < height; gy++) {
        if (rmargin > 0) {
            int ys, ye, ycov;
            cc_heat_row_span(rows, height, gy, &ys, &ye, &ycov);
            char lbl[256] = "";
            if (ycov && s.row_labels != NULL && s.row_labels[ys] != NULL) {
                snprintf(lbl, sizeof(lbl), "%*s", rmargin, s.row_labels[ys]);
            } else {
                memset(lbl, ' ', (size_t)rmargin);
                lbl[rmargin] = '\0';
            }
            memcpy(w, lbl, (size_t)rmargin);
            w += (size_t)rmargin;
        }
        for (gx = 0; gx < width; gx++) {
            const char* cell = columns + ((size_t)gy * (size_t)width + (size_t)gx) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }

    if (footer) {
        if (rmargin > 0) {
            memset(w, ' ', (size_t)rmargin);
            w += (size_t)rmargin;
        }
        for (gx = 0; gx < width; gx++) {
            int xs, xe, xcov;
            cc_heat_col_span(cols, width, gx, &xs, &xe, &xcov);
            if (!xcov) { *w++ = ' '; continue; }
            int span = xe - xs;
            if (span < 1) span = 1;
            const char* T = (s.col_labels != NULL && s.col_labels[xs] != NULL)
                            ? s.col_labels[xs] : "";
            size_t n = strlen(T);
            if (n > (size_t)span) n = (size_t)span;
            if (n > 0) { memcpy(w, T, n); w += n; }
            if ((size_t)span > n) {
                memset(w, ' ', (size_t)span - n);
                w += (size_t)span - n;
            }
        }
        *w++ = '\n';
    }
    *w = '\0';
    free(columns);
    return chart;
}

/* ------------------------------- Box renderer ----------------------------
 * A box plot draws one five-number summary per category. The five values are
 * computed by nearest-rank (see cc_box_create's documented convention) from
 * a sorted copy of each category's samples; the global min/max across every
 * category span the value axis, and each box, its whiskers and its median
 * are placed at 8-sub-pixel rows. The horizontal placement and exact glyph
 * mapping are fully specified in cc_box_create. All scratch buffers are
 * heap-allocated (no VLAs) and freed on every path. */

/* A category's five-number summary (computed by cc_box_summary). */
typedef struct cc_box_five {
    double lo;   /* min    */
    double q1;   /* Q1     */
    double md;   /* median */
    double q3;   /* Q3     */
    double hi;   /* max    */
} cc_box_five;

CC_INLINE int cc_box_cmp(const void* a, const void* b) {
    double x = *(const double*)a;
    double y = *(const double*)b;
    return (x > y) - (x < y);
}

/* Sorts s[0..n-1] in place (the caller owns a copy) and returns the
 * nearest-rank five-number summary. The rank indices use integer arithmetic
 * so floor((n-1)*0.25) is exact: idx/4, idx/2, idx*3/4 for idx = n-1. */
CC_INLINE cc_box_five cc_box_summary(double* s, int n) {
    cc_box_five f;
    long long idx = (long long)(n - 1);
    int q1i = (int)(idx / 4);
    int mdi = (int)(idx / 2);
    int q3i = (int)((idx * 3) / 4);
    qsort(s, (size_t)n, sizeof(double), cc_box_cmp);
    f.lo = s[0];
    f.q1 = s[q1i];
    f.md = s[mdi];
    f.q3 = s[q3i];
    f.hi = s[n - 1];
    return f;
}

/* Maps a value to a 0-based sub-pixel row (bottom 0 .. pixel_height - 1),
 * scaling the full value span across the chart height. */
CC_INLINE int cc_box_level(double v, double gmin, double gmax,
                                  int pixel_height) {
    if (gmax == gmin) return pixel_height / 2;
    double r = (v - gmin) / (gmax - gmin);
    int l = (int)(r * (pixel_height - 1));
    if (l < 0) l = 0;
    if (l >= pixel_height) l = pixel_height - 1;
    return l;
}

/* Joins the per-cell strings (columns[(x*height + y)*32 .. +32), y counted
 * from the bottom) into the final box-string, printing rows top-to-bottom.
 * An optional 8-column value axis (show_prices: global max top, global min
 * bottom) is prepended to every row. Allocates with a pointer cursor (no
 * strcat re-scans); caller frees. */
CC_INLINE char* cc_box_assemble(int width, int height, char* columns,
                                       const cc_box_settings_t* s,
                                       double gmin, double gmax) {
    int margin = s->show_prices ? 8 : 0;
    char max_label[16] = "";
    char min_label[16] = "";
    if (margin > 0) {
        snprintf(max_label, sizeof(max_label), "%8.4g", gmax);
        snprintf(min_label, sizeof(min_label), "%8.4g", gmin);
    }
    const int line_len = width + margin;
    /* Bounded by CC_MAX_CELLS: width*height <= 1e6, so this is <= ~32 MB. */
    size_t total_size = (size_t)line_len * (size_t)height * 32 + (size_t)height + 1;
    char* chart = (char*)calloc(total_size, 1);
    if (chart == NULL) return NULL;

    char* w = chart;
    for (int y = height - 1; y >= 0; y--) {
        if (margin > 0) {
            char mlabel[16] = "";
            if (y == height - 1) snprintf(mlabel, sizeof(mlabel), "%8s", max_label);
            else if (y == 0)     snprintf(mlabel, sizeof(mlabel), "%8s", min_label);
            else { memset(mlabel, ' ', 8); mlabel[8] = '\0'; }
            size_t ml = strlen(mlabel);
            memcpy(w, mlabel, ml);
            w += ml;
        }
        for (int x = 0; x < width; x++) {
            const char* cell = columns + ((size_t)x * height + (size_t)y) * 32;
            size_t cl = strlen(cell);
            memcpy(w, cell, cl);
            w += cl;
        }
        *w++ = '\n';
    }
    *w = '\0';
    return chart;
}

CC_INLINE char* cc_box_create(const cc_box_category_t* cats, int cat_count,
                              int width, int height,
                              const cc_box_settings_t* settings) {
    int i, w, r;
    long long off = 0;
    long long total = 0;
    if (cats == NULL || cat_count <= 0 || !cc_dim_ok(width, height)) {
        return (char*)calloc(1, sizeof(char));
    }
    /* A category with no samples has no well-defined five-number summary, so
     * one empty category empties the whole chart. */
    for (i = 0; i < cat_count; i++) {
        if (cats[i].samples == NULL || cats[i].n <= 0) {
            return (char*)calloc(1, sizeof(char));
        }
        total += cats[i].n;
    }

    cc_box_settings_t s;
    /* All pointer/int fields: a partial {0} brace initializer matches NULL,
     * so there is no sentinel ambiguity (unlike the histogram's doubles). */
    s.rise_color = (settings && settings->rise_color) ? settings->rise_color
                                                      : CC_COLOR_GREEN;
    s.area_color = (settings && settings->area_color &&
                    settings->area_color[0] != '\0')
                   ? settings->area_color : s.rise_color;
    s.bg_color   = settings ? settings->bg_color : NULL;
    s.show_prices = settings ? settings->show_prices : 0;
    if (s.show_prices < 0) s.show_prices = 0;

    double* scratch = (double*)malloc((size_t)total * sizeof(double));
    cc_box_five* fives = (cc_box_five*)malloc((size_t)cat_count * sizeof(cc_box_five));
    char* columns = (char*)malloc((size_t)width * (size_t)height * 32);
    if (scratch == NULL || fives == NULL || columns == NULL) {
        free(scratch); free(fives); free(columns);
        return NULL;
    }

    double gmin = 0.0, gmax = 0.0;
    int have_minmax = 0;
    for (i = 0; i < cat_count; i++) {
        memcpy(scratch + off, cats[i].samples,
               (size_t)cats[i].n * sizeof(double));
        fives[i] = cc_box_summary(scratch + off, cats[i].n);
        if (!have_minmax || fives[i].lo < gmin) gmin = fives[i].lo;
        if (!have_minmax || fives[i].hi > gmax) gmax = fives[i].hi;
        have_minmax = 1;
        off += cats[i].n;
    }

    int pixel_height = height * 8;

    for (w = 0; w < width; w++) {
        int cs = (int)(((long long)w * cat_count) / width);
        cc_box_five f = fives[cs];
        int lo  = cc_box_level(f.lo, gmin, gmax, pixel_height) / 8;
        int q1  = cc_box_level(f.q1, gmin, gmax, pixel_height) / 8;
        int md  = cc_box_level(f.md, gmin, gmax, pixel_height) / 8;
        int q3  = cc_box_level(f.q3, gmin, gmax, pixel_height) / 8;
        int hi  = cc_box_level(f.hi, gmin, gmax, pixel_height) / 8;
        int lq1 = cc_box_level(f.q1, gmin, gmax, pixel_height);
        int lq3 = cc_box_level(f.q3, gmin, gmax, pixel_height);
        const char* wc = s.area_color;
        const char* bc = s.rise_color;
        char* col = columns + (size_t)w * (size_t)height * 32;
        const char* reset_b = cc_reset_for(bc, NULL);
        const char* reset_w = cc_reset_for(wc, NULL);

        for (r = 0; r < height; r++) {
            char* cell = col + (size_t)r * 32;
            int in_box = (q1 <= r && r <= q3);
            int in_whisker = (lo <= r && r <= hi);
            const char* boxglyph = CC_BLOCK_FULL;

            if (in_box) {
                /* Median (rule 1) is a solid full line even on a partial
                 * top edge; interior (rule 2) is a full block. */
                if (r == md) {
                    boxglyph = CC_BLOCK_FULL;
                } else if (r == q3 && q1 < q3) {
                    /* top edge (rule 3): the Q3 sub-pixel, precise. */
                    boxglyph = cc_lower_eighth((lq3 % 8) + 1);
                } else if (r == q1 && q1 == q3) {
                    /* single-row box (rule 4): Q1..Q3 height. */
                    boxglyph = cc_lower_eighth(lq3 - lq1 + 1);
                } else {
                    boxglyph = CC_BLOCK_FULL;   /* bottom edge / interior */
                }
                snprintf(cell, 32, "%s%s%s", CC_S(bc), boxglyph, reset_b);
            } else if (in_whisker) {
                snprintf(cell, 32, "%s%s%s", CC_S(wc), CC_LINE_VERTICAL, reset_w);
            } else if (s.bg_color != NULL) {
                snprintf(cell, 32, "%s %s", s.bg_color,
                         cc_reset_for(s.bg_color, NULL));
            } else {
                snprintf(cell, 32, " ");
            }
        }
    }

    char* chart = cc_box_assemble(width, height, columns, &s, gmin, gmax);
    free(scratch);
    free(fives);
    free(columns);
    return chart;
}

#endif /* CCHARTS_IMPLEMENTATION */
#endif /* CCHARTS_H */