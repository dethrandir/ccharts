/*
 * ccharts_abi.h — flat C ABI for the ccharts single-header library.
 *
 * ccharts turns financial OHLC data into a string; this layer is what lets a
 * language other than C ask it to.
 *
 * WHY THIS EXISTS
 * ---------------
 * ccharts.h exports nothing: CC_INLINE makes every function `static inline`,
 * so the implementation is compiled into whichever translation unit defines
 * CCHARTS_IMPLEMENTATION. `struct cc_ohlc` is likewise private to that block.
 * Neither is reachable from a foreign function interface. This layer defines
 * CCHARTS_IMPLEMENTATION once, and re-exports a stable, cdecl, opaque-handle
 * API that Go (cgo), Rust, C#, Java and WebAssembly can all bind to without
 * repeating the same marshalling and ownership logic five times.
 *
 * CONTRACT
 * --------
 *   - Every entry point returns a ccharts_status; CCHARTS_OK is 0.
 *   - Data handles are opaque, immutable once built, and thread-safe to share
 *     (ccharts.h holds no mutable global state). Release with
 *     ccharts_data_free.
 *   - Rendered strings are heap-allocated by the library and released with
 *     ccharts_string_free. The expected binding pattern is: render, copy into
 *     a native string, free immediately.
 *   - Invalid dimensions are an error here, not the empty string ccharts.h
 *     returns, so the "empty means invalid" convention never reaches a
 *     binding's users.
 *   - NaN and inf are rejected at the boundary: cc_pixel() feeds values to
 *     lround(), which is undefined for non-finite input.
 *
 * All structs are fixed-layout, no bitfields, and nothing is passed by value,
 * which keeps the ABI describable in every foreign-function tool.
 */

#ifndef CCHARTS_ABI_H
#define CCHARTS_ABI_H

#include <stddef.h>
#include <stdint.h>

#define CCHARTS_VERSION "0.2.2"

#if defined(_WIN32)
#  if defined(CCHARTS_ABI_BUILD_SHARED)
#    define CCHARTS_API __declspec(dllexport)
#  elif defined(CCHARTS_ABI_USE_SHARED)
#    define CCHARTS_API __declspec(dllimport)
#  else
#    define CCHARTS_API
#  endif
#elif defined(__GNUC__)
#  define CCHARTS_API __attribute__((visibility("default")))
#else
#  define CCHARTS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* An opaque, immutable OHLC dataset. */
typedef struct ccharts_data ccharts_data;

/* Status codes. Functions return int32_t rather than this enum: an enum's
 * underlying type is implementation-defined, and every foreign function
 * interface would have to guess it. */
typedef enum ccharts_status {
    CCHARTS_OK = 0,
    CCHARTS_ERR_INVALID_ARG = 1,   /* NULL pointer, n <= 0, ... */
    CCHARTS_ERR_PARSE = 2,         /* malformed or empty JSON/CSV */
    CCHARTS_ERR_NOMEM = 3,
    CCHARTS_ERR_NON_FINITE = 4,    /* NaN or inf among the prices */
    CCHARTS_ERR_DIMENSIONS = 5     /* width/height <= 0 or over the limits */
} ccharts_status;

/* Indices for ccharts_color(). Bindings name these in their own enums instead
 * of copying the ANSI escape sequences. */
typedef enum ccharts_color_index {
    CCHARTS_COLOR_BLACK = 0, CCHARTS_COLOR_RED, CCHARTS_COLOR_GREEN,
    CCHARTS_COLOR_YELLOW, CCHARTS_COLOR_BLUE, CCHARTS_COLOR_MAGENTA,
    CCHARTS_COLOR_CYAN, CCHARTS_COLOR_WHITE,
    CCHARTS_COLOR_BRIGHT_BLACK, CCHARTS_COLOR_BRIGHT_RED,
    CCHARTS_COLOR_BRIGHT_GREEN, CCHARTS_COLOR_BRIGHT_YELLOW,
    CCHARTS_COLOR_BRIGHT_BLUE, CCHARTS_COLOR_BRIGHT_MAGENTA,
    CCHARTS_COLOR_BRIGHT_CYAN, CCHARTS_COLOR_BRIGHT_WHITE,
    CCHARTS_COLOR_RESET,
    CCHARTS_COLOR_COUNT
} ccharts_color_index;

/* Rendering options. Every color is a NUL-terminated ANSI escape string or
 * NULL for the library default, exactly like cc_settings_t; pass NULL for the
 * whole struct to take every default. Arbitrary escapes (256-color,
 * truecolor) are accepted — the strings are not interpreted here. */
typedef struct ccharts_settings {
    const char* rise_color;
    const char* fall_color;
    const char* bg_color;
    const char* area_color;
    int32_t single_color;
    int32_t show_prices;
    int32_t show_times;
} ccharts_settings;

/* ---------------------------- Building data ---------------------------- */

/* Builds a dataset from four equal-length price columns. `ts` holds epoch
 * seconds and may be NULL (all timestamps unknown). The arrays are copied
 * immediately and never retained, which keeps cgo's pointer-passing rules and
 * every GC's object lifetime out of the picture. */
CCHARTS_API int32_t ccharts_from_arrays(const double* open,
                                               const double* high,
                                               const double* low,
                                               const double* close,
                                               const int64_t* ts,
                                               int32_t n,
                                               ccharts_data** out);

/* Builds a dataset from the fixed-schema JSON accepted by cc_json_to_ohlc. */
CCHARTS_API int32_t ccharts_parse_json(const char* json,
                                              ccharts_data** out);

/* Builds a dataset from CSV: open,high,low,close[,timestamp] per line.
 * Unlike cc_str_to_ohlc, which fills a caller-sized array and never reports
 * how many rows it actually parsed, this counts the parseable lines first, so
 * the dataset never carries trailing zero-filled candles. */
CCHARTS_API int32_t ccharts_parse_csv(const char* csv,
                                             char value_separator,
                                             char line_separator,
                                             ccharts_data** out);

/* Number of candles, or 0 for NULL. */
CCHARTS_API int32_t ccharts_data_len(const ccharts_data* data);

/* Releases a dataset. NULL is a no-op. */
CCHARTS_API void ccharts_data_free(ccharts_data* data);

/* ------------------------------ Rendering ------------------------------ */

/* Renders a line chart. On CCHARTS_OK, *out points to a NUL-terminated UTF-8
 * string owned by the caller (release with ccharts_string_free) and *out_len,
 * when not NULL, receives its length in bytes. On error *out is set to NULL. */
CCHARTS_API int32_t ccharts_line(const ccharts_data* data,
                                        int32_t width, int32_t height,
                                        const ccharts_settings* settings,
                                        char** out, size_t* out_len);

/* Renders a candlestick chart; same contract as ccharts_line. */
CCHARTS_API int32_t ccharts_candle(const ccharts_data* data,
                                          int32_t width, int32_t height,
                                          const ccharts_settings* settings,
                                          char** out, size_t* out_len);

/* Releases a string returned by ccharts_line / ccharts_candle /
 * ccharts_pie_from_slices. */
CCHARTS_API void ccharts_string_free(char* s);

/* -------------------------------- Pie -------------------------------- */

/* One pie slice: a label (may be NULL or empty) and a positive amount. The
 * renderer turns the values into percentages, so they are amounts, not
 * fractions. Mirrors cc_pie_slice_t in ccharts.h (the two structs stay
 * separate: the header channels are internally linked, this one is the FFI
 * surface, and layout is documented here rather than shared). */
typedef struct ccharts_pie_slice {
    const char* label;
    double value;
} ccharts_pie_slice;

/* Renders a pie/donut chart of `count` slices and returns it in *out (a
 * NUL-terminated UTF-8 string the caller releases with ccharts_string_free;
 * *out_len, when not NULL, receives its length in bytes). `donut` hollows
 * the center; `colors` is an array of `color_count` ANSI escape strings used
 * per slice (index mod color_count), or NULL for the fixed default palette;
 * `show_legend` appends one "label  value (pct%)" line per slice below the
 * disk when `show_pct` is set, otherwise without the percentage.
 *
 * The remaining arguments are the optional Fas 3 pie settings, all of which
 * default to the original behavior when left at their sentinel/zero values:
 *   - slice_gap          : angular gap between slices, radians; 0 = adjacent
 *   - inner_radius_ratio : donut thickness in [0,1]; 0 = disk; NEGATIVE
 *                          (e.g. -1) = unspecified, so `donut` decides (0.5
 *                          for a donut, 0 for a disk); >1 clamped to 1
 *   - legend_format      : ccharts_pie_legend_format enum; 0 = original
 *   - start_angle        : radians where slice 0 begins; NEGATIVE =
 *                          unspecified (CC_PI/2 = 12 o'clock)
 *   - counter_clockwise  : 0 = original counter-clockwise sweep; nonzero =
 *                          mirrored (clockwise) sweep
 *   - center_text        : text drawn in the hollow center (only when there is
 *                          a hollow); NULL or "" disables it
 *
 * Non-finite slice_gap / inner_radius_ratio / start_angle are rejected.
 *
 * Same contract as ccharts_line: CCHARTS_OK with *out set to a string on
 * success (including the empty string the header returns when every slice
 * value is <= 0), CCHARTS_ERR_INVALID_ARG for NULL slices / count <= 0,
 * CCHARTS_ERR_NON_FINITE for NaN/inf values, CCHARTS_ERR_DIMENSIONS for
 * width/height outside cc_dim_ok, CCHARTS_ERR_NOMEM on allocation failure.
 * On error *out is set to NULL. */
CCHARTS_API int32_t ccharts_pie_from_slices(
    const ccharts_pie_slice* slices, int32_t count,
    int32_t width, int32_t height,
    int32_t donut,
    const char* const* colors, int32_t color_count,
    int32_t show_legend, int32_t show_pct,
    double slice_gap, double inner_radius_ratio, int32_t legend_format,
    double start_angle, int32_t counter_clockwise, const char* center_text,
    char** out, size_t* out_len);

/* legend_format values for ccharts_pie_from_slices, mirroring the header's
 * CC_PIE_LEGEND_* constants. Unknown values fall back to VALUE (original). */
#define CCHARTS_PIE_LEGEND_VALUE      0 /* "label  value" (+ "(NN%)" when show_pct) */
#define CCHARTS_PIE_LEGEND_LABEL_PCT  1 /* "label  NN%"                             */
#define CCHARTS_PIE_LEGEND_VALUE_PCT  2 /* "value  (NN%)"                           */
#define CCHARTS_PIE_LEGEND_LABEL      3 /* "label" only                             */

/* ------------------------------ Histogram ------------------------------ */

/* Histogram rendering options. Every color is a NUL-terminated ANSI escape
 * string or NULL for the library default, exactly like ccharts_settings;
 * pass NULL for the whole struct to take every default. `bin_count` <= 0
 * auto-selects (20 bins for >= 40 samples, else 10, trimmed to the width).
 * `min_value`/`max_value` are the value window for the histogram: NaN means
 * auto-select that endpoint from the data range (so NaN is allowed, unlike
 * the samples). `show_bins` appends a value-axis footer row (window min
 * left, window max right); `show_prices` prepends an 8-column left margin
 * with the max-count / min-count labels. Mirrors cc_hist_settings_t in
 * ccharts.h (the two structs stay separate: the header channels are
 * internally linked, this one is the FFI surface). */
typedef struct ccharts_hist_settings {
    const char* rise_color;
    const char* bg_color;
    int32_t bin_count;
    double  min_value;
    double  max_value;
    int32_t show_bins;
    int32_t show_prices;
} ccharts_hist_settings;

/* Renders a histogram of `count` scalar samples (a 1-D sequence, not OHLC
 * rows) into a `width` x `height` grid. Same contract as ccharts_line:
 * CCHARTS_OK with *out (NUL-terminated UTF-8, release with
 * ccharts_string_free) on success, CCHARTS_ERR_INVALID_ARG for NULL
 * samples / count <= 0 / NULL out, CCHARTS_ERR_NON_FINITE for NaN/inf
 * samples or +-inf min_value/max_value (NaN min/max is the "auto" sentinel
 * and is allowed), CCHARTS_ERR_DIMENSIONS for width/height outside
 * cc_dim_ok, CCHARTS_ERR_NOMEM on allocation failure. On error *out is set
 * to NULL. */
CCHARTS_API int32_t ccharts_hist(const double* samples, int32_t count,
                                 int32_t width, int32_t height,
                                 const ccharts_hist_settings* settings,
                                 char** out, size_t* out_len);

/* ------------------------------ Sparkline ------------------------------ */

/* Sparkline rendering options. Mirrors cc_spark_settings_t in ccharts.h (the
 * two structs stay separate: the header channels are internally linked, this
 * one is the FFI surface, and layout is documented here rather than shared).
 * Every color is a NUL-terminated ANSI escape string or NULL for the library
 * default; pass NULL for the whole struct to take every default. `min_above`
 * / `min_below` reserve that many sub-pixels at the top/bottom edge so the
 * line does not clip; both are plain ints defaulting to 0, so a partial {0}
 * initializer means the same as NULL (no sentinel ambiguity). */
typedef struct ccharts_spark_settings {
    const char* rise_color;
    const char* area_color;
    int32_t min_above;
    int32_t min_below;
} ccharts_spark_settings;

/* Renders a sparkline of `count` scalar samples (a 1-D sequence of
 * close-like values, not OHLC rows) into a `width` x `height` grid. Same
 * contract as ccharts_line: CCHARTS_OK with *out (NUL-terminated UTF-8,
 * release with ccharts_string_free) on success, CCHARTS_ERR_INVALID_ARG for
 * NULL samples / count <= 0 / NULL out, CCHARTS_ERR_NON_FINITE for NaN/inf
 * samples, CCHARTS_ERR_DIMENSIONS for width/height outside cc_dim_ok,
 * CCHARTS_ERR_NOMEM on allocation failure. On error *out is set to NULL. */
CCHARTS_API int32_t ccharts_spark(const double* samples, int32_t count,
                                  int32_t width, int32_t height,
                                  const ccharts_spark_settings* settings,
                                  char** out, size_t* out_len);

/* ------------------------------ Bar chart ------------------------------ */

/* One bar: a categorical label (may be NULL or empty) and a non-negative
 * height. Values are clamped to zero by the renderer (a negative value draws
 * that bar at zero height rather than below the axis), so only non-finite
 * values are an error. Mirrors cc_bar_item_t in ccharts.h (the two structs
 * stay separate: the header channels are internally linked, this one is the
 * FFI surface, and layout is documented here rather than shared). */
typedef struct ccharts_bar_slice {
    const char* label;
    double value;
} ccharts_bar_slice;

/* Bar chart rendering options. Mirrors cc_bar_settings_t in ccharts.h (the
 * two structs stay separate: the header channels are internally linked, this
 * one is the FFI surface). Every color is a NUL-terminated ANSI escape string
 * or NULL for the library default; pass NULL for the whole struct to take
 * every default. `show_labels` appends a footer row with each column's label
 * (truncated to the column width); `show_prices` prepends an 8-column value
 * axis with the max bar value at the top and 0 (the baseline) at the bottom.
 * All fields are pointers or plain ints, so a partial {0} initializer means
 * the same as NULL (no sentinel ambiguity). */
typedef struct ccharts_bar_settings {
    const char* rise_color;
    const char* bg_color;
    int32_t show_labels;
    int32_t show_prices;
} ccharts_bar_settings;

/* Renders a categorical bar chart of `count` (label, value) pairs into a
 * `width` x `height` grid. Each bar grows up from a zero baseline scaled to
 * the largest value, drawn with 8 sub-pixel rows (cc_lower_eighth tops). Same
 * contract as ccharts_line: CCHARTS_OK with *out (NUL-terminated UTF-8,
 * release with ccharts_string_free) on success, CCHARTS_ERR_INVALID_ARG for
 * NULL items / count <= 0 / NULL out, CCHARTS_ERR_NON_FINITE for NaN/inf
 * values, CCHARTS_ERR_DIMENSIONS for width/height outside cc_dim_ok,
 * CCHARTS_ERR_NOMEM on allocation failure. Negative values are clamped to
 * zero (not an error). On error *out is set to NULL. */
CCHARTS_API int32_t ccharts_bar(const ccharts_bar_slice* items, int32_t count,
                                int32_t width, int32_t height,
                                const ccharts_bar_settings* settings,
                                char** out, size_t* out_len);

/* ---------------------------- Stacked bar chart -------------------------- */

/* One stacked-bar series: a name (may be NULL/empty; the per-series color
 * comes from the palette) and a `values` array with one entry per category.
 * All series must share the same category count (settings->cats). Values are
 * clamped to zero by the renderer (a negative entry draws at zero height
 * rather than below the axis), so only non-finite values are an error.
 * Mirrors cc_stack_series_t in ccharts.h (the two structs stay separate: the
 * header channels are internally linked, this one is the FFI surface, and
 * layout is documented here rather than shared). */
typedef struct ccharts_stack_series {
    const char* name;
    const double* values;
} ccharts_stack_series;

/* Stacked bar rendering options. Mirrors cc_stack_settings_t in ccharts.h.
 * `colors` is a NULL-terminated per-series palette override (or NULL for the
 * fixed default palette); `cat_labels` is an optional array of `cats` category
 * names used for the label footer; `series` repeats series_count and `cats` is
 * the number of categories (the length of every series' values array) and is
 * REQUIRED. `show_labels` appends a footer row with each column's category
 * label (truncated to the column width); `show_prices` prepends an 8-column
 * value axis with the tallest stack total at the top and 0 (the baseline) at
 * the bottom. All fields are pointers or plain ints, so a partial {0}
 * initializer means the same as NULL (no sentinel ambiguity). */
typedef struct ccharts_stack_settings {
    const char* const* colors;
    const char* bg_color;
    const char* const* cat_labels;
    int32_t series;
    int32_t cats;
    int32_t show_labels;
    int32_t show_prices;
} ccharts_stack_settings;

/* Renders a stacked bar chart of `series_count` series (the 2-D matrix: each
 * series carries its own `values` array of settings->cats entries) into a
 * `width` x `height` grid. Each category's bar is the vertical SUM of its
 * series' values, drawn as stacked segments (one per series) with 8 sub-pixel
 * rows, colored via a deterministic palette or the settings override. Same
 * contract as ccharts_line: CCHARTS_OK with *out (NUL-terminated UTF-8,
 * release with ccharts_string_free) on success, CCHARTS_ERR_INVALID_ARG for
 * NULL series / series_count <= 0 / NULL settings / NULL out,
 * CCHARTS_ERR_NON_FINITE for NaN/inf values, CCHARTS_ERR_DIMENSIONS for
 * width/height outside cc_dim_ok, CCHARTS_ERR_NOMEM on allocation failure.
 * Negative values are clamped to zero (not an error). On error *out is set to
 * NULL. */
CCHARTS_API int32_t ccharts_stack(const ccharts_stack_series* series,
                                  int32_t series_count,
                                  int32_t width, int32_t height,
                                  const ccharts_stack_settings* settings,
                                  char** out, size_t* out_len);

/* ------------------------------ Heatmap chart ---------------------------- */

/* Heatmap rendering options. Mirrors cc_heat_settings_t in ccharts.h.
 * `low_color` is the ANSI color for the minimum value (default bright-black),
 * `high_color` for the maximum (default bright-white), and `mid_color` is an
 * optional ANSI color that replaces the ladder's middle entry (index 5) for a
 * 3-stop ramp (NULL = 2-stop). `bg_color` colors the grid cells the matrix
 * does not cover (matrix smaller than width/height). `row_labels` / 
 * `col_labels` are optional `rows` / `cols` label arrays printed around the
 * grid when `show_labels` is set. All fields are pointers or plain ints, so a
 * partial {0} initializer means the same as NULL (no sentinel ambiguity). */
typedef struct ccharts_heat_settings {
    const char* low_color;
    const char* high_color;
    const char* mid_color;
    const char* bg_color;
    const char* const* row_labels;
    const char* const* col_labels;
    int32_t show_labels;
} ccharts_heat_settings;

/* Renders a heatmap of a `rows` x `cols` row-major `values` matrix into a
 * `width` x `height` grid. Matrix elements map to the fixed deterministic
 * colormap ladder by their value's position between the matrix min/max; a
 * matrix larger than the grid is downsampled by block-average, and one
 * smaller than the grid occupies the top-left with background padding. Same
 * contract as ccharts_line: CCHARTS_OK with *out (NUL-terminated UTF-8,
 * release with ccharts_string_free) on success, CCHARTS_ERR_INVALID_ARG for
 * NULL values / rows <= 0 / cols <= 0 / NULL out, CCHARTS_ERR_NON_FINITE for
 * NaN/inf values, CCHARTS_ERR_DIMENSIONS for width/height outside cc_dim_ok,
 * CCHARTS_ERR_NOMEM on allocation failure. On error *out is set to NULL. */
CCHARTS_API int32_t ccharts_heat(const double* values, int32_t rows,
                                 int32_t cols,
                                 int32_t width, int32_t height,
                                 const ccharts_heat_settings* settings,
                                 char** out, size_t* out_len);

/* ---------------------------- Introspection ---------------------------- */

/* ANSI escape for a ccharts_color_index, or NULL when out of range. */
CCHARTS_API const char* ccharts_color(int32_t index);

/* Human-readable message for a status code; never NULL. */
CCHARTS_API const char* ccharts_error_message(int32_t status);

/* Library version ("0.2.2"), shared by every binding. */
CCHARTS_API const char* ccharts_version(void);

/* CC_MAX_DIM / CC_MAX_CELLS, so bindings do not duplicate the limits. */
CCHARTS_API int32_t ccharts_max_dim(void);
CCHARTS_API int32_t ccharts_max_cells(void);

#ifdef __cplusplus
}
#endif

#endif /* CCHARTS_ABI_H */
