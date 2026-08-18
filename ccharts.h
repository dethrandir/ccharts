/*
 * ccharts.h — single-header terminal charts for financial OHLC data.
 *
 * Renders line and candlestick charts as ANSI-colored strings of Unicode
 * block characters that can be printed straight to a terminal.
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
cc_settings_t cc_settings_resolve(const cc_settings_t* settings);

/* Parses CSV text into a heap-allocated cc_ohlc_t array.
 *
 * Format: one candle per line, fields separated by `val_seperator`:
 *     open,high,low,close          (timestamp = 0)
 *     open,high,low,close,timestamp (ISO8601 or epoch seconds)
 *
 * `size` is the expected number of lines. On success returns 0 and stores a
 * calloc'd array of exactly `size` entries in *ohlc (caller frees it).
 * Returns non-zero on allocation failure. */
int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc,
                   char val_seperator, char line_seperator);

/* Parses a fixed-schema JSON document into a heap-allocated cc_ohlc_t array.
 *
 * Expected schema — an array of objects with string timestamps:
 *     [{"ts":"2026-07-20T00:00:00+00:00","open":328.75,"high":330.0,
 *       "low":323.75,"close":328.0,"volume":46622936}, ...]
 * The "volume" field is ignored. On success returns 0 and stores the number
 * of candles in *size and a calloc'd array in *ohlc (caller frees both).
 * Returns non-zero on malformed input or allocation failure. */
int cc_json_to_ohlc(const char* json, cc_ohlc_t** ohlc, int* size);

/* Renders a smooth line chart of the close prices.
 * `width`/`height` are the chart plot area in cells. Returns a malloc'd
 * string (caller frees) that may additionally contain a left price margin
 * and a time footer depending on the settings flags. */
char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height,
                     const cc_settings_t* settings);

/* Renders a candlestick chart from the OHLC data.
 * `width`/`height` are the chart plot area in cells. When width >= size each
 * candle is a few cells wide with a gap between neighbors; when width < size
 * neighboring candles are aggregated into virtual candles (like the line
 * chart's downsampling). Returns a malloc'd string (caller frees). */
char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height,
                       const cc_settings_t* settings);
#ifdef __cplusplus
extern "C" {
#endif
#endif
#ifdef CCHARTS_IMPLEMENTATION
#ifdef __cplusplus
extern "C" {
#endif

/* One candlestick. `timestamp` is epoch seconds, 0 when unknown. */
struct cc_ohlc {
    double open;
    double high;
    double low;
    double close;
    long long timestamp;
};

inline cc_settings_t cc_settings_resolve(const cc_settings_t* settings) {
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
 *   2. parsing helpers      -> whitespace trim + timestamp conversion
 *   3. cc_str_to_ohlc       -> CSV  -> cc_ohlc_t[]
 *   4. cc_json_field/to_ohlc-> JSON -> cc_ohlc_t[]
 *   5. rendering helpers    -> value-to-pixel math + cell string builders
 *   6. cc_line_create       -> line chart renderer
 *   7. cc_candle_create     -> candle chart renderer
 * ========================================================================== */

static inline char* cc_trim_whitespace(char* str) {
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

static inline long long cc_iso8601_to_epoch(const char* s) {
    /* Parses "YYYY-MM-DDTHH:MM:SS[+ZZ:ZZ]" and converts to epoch seconds
     * using Howard Hinnant's civil_date algorithm (timezone is ignored).
     * Returns 0 for malformed input. */
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) < 3) {
        return 0;
    }
    if (mo < 1 || mo > 12 || d < 1 || d > 31) {
        return 0;
    }

    long long yy = y - (mo <= 2);
    long long era = (yy >= 0 ? yy : yy - 399) / 400;
    long long yoe = yy - era * 400;
    long long doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + doe - 719468;

    return days * 86400 + (long long)h * 3600 + (long long)mi * 60 + se;
}

static inline long long cc_parse_ts(const char* s) {
    /* Accepts either an ISO8601 string ("2026-07-20T21:00:00+00:00") or a
     * plain epoch-seconds number, and returns epoch seconds (0 = none). */
    if (s == NULL || *s == '\0') {
        return 0;
    }
    for (const char* p = s; *p; p++) {
        if (*p == '-' || *p == 'T' || *p == ':') {
            return cc_iso8601_to_epoch(s);
        }
    }
    return (long long)atoll(s);
}

/* ------------------------------ CSV parser ------------------------------ */

inline int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc, char val_seperator, char line_seperator)
{
    *ohlc = (cc_ohlc_t*)calloc(size, sizeof(cc_ohlc_t));
    if (*ohlc == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }

    char tokens[size][256];

    int start = 0;
    int end = 0;
    int i = 0;
    int num = 0;
    while (data[i] != '\0') {
        if (data[i] != line_seperator) {
            i++;
            continue;
        }

        end = i;
        int len = end - start;
        strncpy(tokens[num], data + start, len);
        tokens[num][len] = '\0';
        num++;
        start=end +1;
        i++;

    }

    if (start < i) {
        int len = i - start;
        strncpy(tokens[num], data + start, len);
        tokens[num][len] = '\0';
        num++;
    }

    int token_count = num;
    for (int i = 0; i<token_count;i++) {
        char* token = tokens[i];

        char* trimmed = cc_trim_whitespace(token);
        memmove(token, trimmed, strlen(trimmed) + 1);

        cc_ohlc_t _ohlc = {0};

        int field_count = 1;
        for (int k = 0; token[k] != '\0'; k++) {
            if (token[k] == val_seperator) field_count++;
        }
        if (field_count > 5) field_count = 5;

        int start = 0;
        int end = 0;
        int j = 0;
        int num = 0;
        while (num < field_count) {
            if (token[j] != val_seperator && token[j] != '\0') {
                j++;
                continue;
            }

            end = j;
            int len = end -start;
            char val_str[64];
            strncpy(val_str, token+start, len);
            val_str[len] = '\0';
            start=end +1;
            j++;

            if (num == 4) {
                _ohlc.timestamp = cc_parse_ts(val_str);
            } else {
                double val = atof(val_str);
                if (num == 0) {
                    _ohlc.open=val;
                } else if (num == 1) {
                    _ohlc.high=val;
                } else if (num == 2) {
                    _ohlc.low=val;
                } else if (num == 3) {
                    _ohlc.close=val;
                }
            }

            num++;

        }

        (*ohlc)[i] = _ohlc;
    }

    return 0;
}

/* ------------------------------ JSON parser ------------------------------ */

static inline const char* cc_json_field(const char* start, const char* key, char* buf, size_t buf_n) {
    /* Finds `"key": value` after `start`, copies the value into buf (both
     * quoted strings and bare numbers are handled), and returns the position
     * just past the value, or NULL when the key is absent. */
    const char* p = strstr(start, key);
    if (!p) return NULL;
    p = strchr(p + strlen(key), ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '"') {
        p++;
        const char* e = strchr(p, '"');
        if (!e) return NULL;
        size_t len = (size_t)(e - p);
        if (len >= buf_n) len = buf_n - 1;
        memcpy(buf, p, len);
        buf[len] = '\0';
        return e + 1;
    }
    const char* e = p;
    while (*e && *e != ',' && *e != '}' && *e != ']' && *e != '\n' && *e != '\r' && *e != ' ') e++;
    size_t len = (size_t)(e - p);
    if (len >= buf_n) len = buf_n - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return e;
}

inline int cc_json_to_ohlc(const char* json, cc_ohlc_t** ohlc, int* size) {
    *ohlc = NULL;
    *size = 0;
    if (json == NULL) {
        return 1;
    }

    int count = 0;
    for (const char* p = json; (p = strstr(p, "\"open\"")) != NULL; p += 6) {
        count++;
    }
    if (count <= 0) {
        return 1;
    }

    *ohlc = (cc_ohlc_t*)calloc(count, sizeof(cc_ohlc_t));
    if (*ohlc == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }

    const char* cur = json;
    for (int i = 0; i < count; i++) {
        cc_ohlc_t c = {0};
        char buf[64];

        if (cc_json_field(cur, "\"ts\"", buf, sizeof(buf))) {
            c.timestamp = cc_parse_ts(buf);
        }
        if (cc_json_field(cur, "\"open\"", buf, sizeof(buf))) {
            c.open = atof(buf);
        }
        if (cc_json_field(cur, "\"high\"", buf, sizeof(buf))) {
            c.high = atof(buf);
        }
        if (cc_json_field(cur, "\"low\"", buf, sizeof(buf))) {
            c.low = atof(buf);
        }
        const char* after = cc_json_field(cur, "\"close\"", buf, sizeof(buf));
        if (after) {
            c.close = atof(buf);
            const char* brace = strchr(after, '}');
            cur = brace ? brace + 1 : after;
        }
        (*ohlc)[i] = c;
    }

    *size = count;
    return 0;
}

/* --------------------------- Rendering helpers --------------------------- */

/* Smallest / largest value in arr[0..n-1]. */
static inline double find_min(double arr[], int n) {
	double min = arr[0];
	for (int i =1; i<n;i++) {
		if (arr[i] < min) {
			min = arr[i];
		}
	}
	return min;
}

static inline double find_max(double arr[], int n) {
	double max = arr[0];
	for (int i =1; i<n; i++) {
		if (arr[i] > max) {
			max = arr[i];
		}
	}
	return max;
}

/* Maps a value in [min, max] to a pixel row in [0, pixel_height-1].
 * `range` is max - min; a flat range is centered to avoid divide-by-zero. */
static inline int cc_pixel(double val, double min, double max, double range, int pixel_height) {
    double t = (range == 0.0) ? 0.5 : (val - min) / range;
    int p = (int)lround(t * (pixel_height - 1));
    if (p < 0) p = 0;
    if (p >= pixel_height) p = pixel_height - 1;
    return p;
}

static inline void cc_render_cell(char out[32], unsigned char bodybits, unsigned char wickbits, const char* fg, const char* bg) {
    /* Turns a candle cell's mask bits into a ready-to-print string.
     * bodybits: bit0 = lower half filled, bit1 = upper half filled (body).
     * wickbits: non-zero means a wick passes through this cell. Body wins
     * over wick so a partial body still reads as a solid block. */
    const char *block;
    if (bodybits == 3)          block = CC_BLOCK_FULL;
    else if (bodybits == 2)     block = CC_BLOCK_UPPER_HALF;
    else if (bodybits == 1)     block = CC_BLOCK_LOWER_HALF;
    else if (wickbits != 0)     block = CC_LINE_VERTICAL;
    else                        block = NULL;

    if (block != NULL) {
        if (bg != NULL) {
            snprintf(out, 32, "%s%s%s%s", bg, fg, block, CC_COLOR_RESET);
        } else {
            snprintf(out, 32, "%s%s%s", fg, block, CC_COLOR_RESET);
        }
    } else {
        if (bg != NULL) {
            snprintf(out, 32, "%s %s", bg, CC_COLOR_RESET);
        } else {
            snprintf(out, 32, " ");
        }
    }
}

static cc_ohlc_t cc_agg_ohlc(const cc_ohlc_t* data, int start, int end) {
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

static inline const char* cc_lower_eighth(int n) {
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

static inline const char* cc_time_format(long long first, long long last, int count) {
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

static char* cc_assemble_chart(int width, int height, char columns[width][height][32],
                               const cc_settings_t* s,
                               long long ts_first, long long ts_last, int count,
                               double max, double min) {
    /* Joins the per-cell strings (columns[x][y], y counted from the bottom)
     * into the final chart string, printing rows top-to-bottom. Optionally
     * adds a left price margin (max on the top row, min on the bottom row)
     * and a footer row with the first/last timestamps. Allocates the result
     * with calloc; the caller frees it. */
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
        if (gmtime_r(&a, &tmv)) strftime(t_first, sizeof(t_first), fmt, &tmv);
        if (gmtime_r(&b, &tmv)) strftime(t_last, sizeof(t_last), fmt, &tmv);
        footer = 1;
    }

    size_t total_size = (size_t)(width + margin) * (height + footer) * 32 + height + footer + 1;
    char* chart = (char*)calloc(total_size, sizeof(char));

    for (int y = height - 1; y >= 0; y--) {
        if (margin > 0) {
            char mlabel[16] = "";
            if (y == height - 1) snprintf(mlabel, sizeof(mlabel), "%8s", max_label);
            else if (y == 0)     snprintf(mlabel, sizeof(mlabel), "%8s", min_label);
            else                 snprintf(mlabel, sizeof(mlabel), "        ");
            strcat(chart, mlabel);
        }
        for (int x = 0; x < width; x++) {
            strcat(chart, columns[x][y]);
        }
        strcat(chart, "\n");
    }

    if (footer) {
        int line_len = width + margin;
        char line[512];
        snprintf(line, sizeof(line), "%*s", line_len, "");
        size_t t1 = strlen(t_first);
        if (t1 > (size_t)line_len) t1 = line_len;
        memcpy(line, t_first, t1);
        size_t t2 = strlen(t_last);
        if (t2 > (size_t)line_len) t2 = line_len;
        memcpy(line + line_len - t2, t_last, t2);
        strcat(chart, line);
        strcat(chart, "\n");
    }

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
 * ------------------------------------------------------------------------- */

inline char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height,
                            const cc_settings_t* settings) {

    double closes[size];
    for (int i = 0; i < size; i++) {
        closes[i] = data[i].close;
    }

    double vals[width];
    memset(vals, 0, sizeof(vals));

    for (int w = 0; w < width; w++) {
        int start_idx = (w * size) / width;
        int end_idx = ((w + 1) * size) / width;
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

    cc_settings_t s = cc_settings_resolve(settings);

    const char* col_color[width];
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

    int py[width];
    for (int i = 0; i < width; i++) {
        py[i] = cc_pixel(vals[i], min, max, diff_range, pixel_height);
    }

    char columns[width][height][32];

    for (int i = 0; i < width; i++) {

        int lo = py[i];
        int hi = py[i];
        if (i + 1 < width) {
            if (py[i+1] < lo) lo = py[i+1];
            if (py[i+1] > hi) hi = py[i+1];
        }

        int cell_lo = lo / 8;
        int cell_hi = hi / 8;

        for (int r = 0; r < cell_lo && r < height; r++) {
            if (s.area_color != NULL) {
                snprintf(columns[i][r], sizeof(columns[i][r]), "%s %s", s.area_color, CC_COLOR_RESET);
            } else if (s.bg_color != NULL) {
                snprintf(columns[i][r], sizeof(columns[i][r]), "%s %s", s.bg_color, CC_COLOR_RESET);
            } else {
                snprintf(columns[i][r], sizeof(columns[i][r]), " ");
            }
        }

        for (int r = cell_lo; r < cell_hi && r < height; r++) {
            snprintf(columns[i][r], sizeof(columns[i][r]), "%s%s%s", col_color[i], CC_BLOCK_FULL, CC_COLOR_RESET);
        }

        if (cell_hi >= 0 && cell_hi < height) {
            int count = hi - cell_hi * 8 + 1;
            if (count > 8) count = 8;
            snprintf(columns[i][cell_hi], sizeof(columns[i][cell_hi]), "%s%s%s",
                     col_color[i], cc_lower_eighth(count), CC_COLOR_RESET);
        }

        for (int r = cell_hi + 1; r < height; r++) {
            if (s.bg_color != NULL) {
                snprintf(columns[i][r], sizeof(columns[i][r]), "%s %s", s.bg_color, CC_COLOR_RESET);
            } else {
                snprintf(columns[i][r], sizeof(columns[i][r]), " ");
            }
        }
    }

    long long ts_first = (size > 0) ? data[0].timestamp : 0;
    long long ts_last = (size > 0) ? data[size-1].timestamp : 0;
    return cc_assemble_chart(width, height, columns, &s,
                             ts_first, ts_last, size, max, min);
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
 * ------------------------------------------------------------------------- */

inline char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height,
                              const cc_settings_t* settings) {
    if (size <= 0 || width <= 0 || height <= 0) {
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

    unsigned char body_mask[width][height];
    unsigned char wick_mask[width][height];
    memset(body_mask, 0, sizeof(body_mask));
    memset(wick_mask, 0, sizeof(wick_mask));

    const char* col_color[width];

    if (width >= size) {

        for (int i = 0; i < size; i++) {
            int cs = (i * width) / size;
            int ce = ((i + 1) * width) / size;
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
                    for (int p = blo; p <= bhi; p++) {
                        body_mask[c][p / 2] |= (p % 2) ? 2 : 1;
                    }
                }
            }

            for (int p = pl; p <= ph; p++) {
                wick_mask[cmid][p / 2] |= (p % 2) ? 2 : 1;
            }
        }
    } else {

        for (int w = 0; w < width; w++) {
            int ss = (w * size) / width;
            int se = ((w + 1) * size) / width;
            if (se <= ss) se = ss + 1;

            cc_ohlc_t v = cc_agg_ohlc(data, ss, se);
            col_color[w] = (v.close >= v.open) ? s.rise_color : s.fall_color;

            int po = cc_pixel(v.open, min, max, range, pixel_height);
            int pc = cc_pixel(v.close, min, max, range, pixel_height);
            int ph = cc_pixel(v.high, min, max, range, pixel_height);
            int pl = cc_pixel(v.low, min, max, range, pixel_height);

            int blo = (po < pc) ? po : pc;
            int bhi = (po > pc) ? po : pc;

            for (int p = blo; p <= bhi; p++) {
                body_mask[w][p / 2] |= (p % 2) ? 2 : 1;
            }
            for (int p = pl; p <= ph; p++) {
                wick_mask[w][p / 2] |= (p % 2) ? 2 : 1;
            }
        }
    }

    char columns[width][height][32];
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            cc_render_cell(columns[i][j], body_mask[i][j], wick_mask[i][j], col_color[i], s.bg_color);
        }
    }

    long long ts_first = (size > 0) ? data[0].timestamp : 0;
    long long ts_last = (size > 0) ? data[size-1].timestamp : 0;
    return cc_assemble_chart(width, height, columns, &s,
                             ts_first, ts_last, size, max, min);
}

#ifdef __cplusplus
}
#endif
#endif
