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
                snprintf(cell, 32, "%s %s", s.area_color, CC_COLOR_RESET);
            } else if (s.bg_color != NULL) {
                snprintf(cell, 32, "%s %s", s.bg_color, CC_COLOR_RESET);
            } else {
                snprintf(cell, 32, " ");
            }
        }

        for (int r = cell_lo; r < cell_hi && r < height; r++) {
            snprintf(col + (size_t)r * 32, 32, "%s%s%s", col_color[i], CC_BLOCK_FULL, CC_COLOR_RESET);
        }

        if (cell_hi >= 0 && cell_hi < height) {
            int count = hi - cell_hi * 8 + 1;
            if (count > 8) count = 8;
            snprintf(col + (size_t)cell_hi * 32, 32, "%s%s%s",
                     col_color[i], cc_lower_eighth(count), CC_COLOR_RESET);
        }

        for (int r = cell_hi + 1; r < height; r++) {
            char* cell = col + (size_t)r * 32;
            if (s.bg_color != NULL) {
                snprintf(cell, 32, "%s %s", s.bg_color, CC_COLOR_RESET);
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

#endif /* CCHARTS_IMPLEMENTATION */
#endif /* CCHARTS_H */