# C API & ABI Reference

`ccharts` provides two layers of C interfaces:
1. **Single-Header C Library (`ccharts.h`)**: Header-only implementation with static internal linkage, ideal for direct inclusion in C/C++ projects.
2. **Flat C ABI (`abi/ccharts_abi.h` / `abi/ccharts_abi.c`)**: Dynamic/static linkable C interface with opaque handles and explicit status codes, designed for Foreign Function Interfaces (FFI).

---

## 1. Single-Header Library (`ccharts.h`)

### Usage Model
In exactly **one** translation unit, define `CCHARTS_IMPLEMENTATION` before including `ccharts.h`:

```c
#define CCHARTS_IMPLEMENTATION
#include "ccharts.h"
```

In other translation units, simply `#include "ccharts.h"` without the macro to access function prototypes and struct declarations.

Compile with `-lm` (linking the standard math library):
```sh
gcc -O3 main.c -lm -o my_app
```

---

### Core Data Structures & Constants

#### Macros & Limits
```c
#define CC_MAX_DIM    100000    /* Max width or height in cells */
#define CC_MAX_CELLS  1000000   /* Max total cells (width * height) */

#define CC_COLOR_RESET           "\x1b[0m"
#define CC_COLOR_BLACK           "\x1b[30m"
#define CC_COLOR_RED             "\x1b[31m"
#define CC_COLOR_GREEN           "\x1b[32m"
#define CC_COLOR_YELLOW          "\x1b[33m"
#define CC_COLOR_BLUE            "\x1b[34m"
#define CC_COLOR_MAGENTA         "\x1b[35m"
#define CC_COLOR_CYAN            "\x1b[36m"
#define CC_COLOR_WHITE           "\x1b[37m"
#define CC_COLOR_BRIGHT_BLACK    "\x1b[90m"
#define CC_COLOR_BRIGHT_RED      "\x1b[91m"
#define CC_COLOR_BRIGHT_GREEN    "\x1b[92m"
#define CC_COLOR_BRIGHT_YELLOW   "\x1b[93m"
#define CC_COLOR_BRIGHT_BLUE     "\x1b[94m"
#define CC_COLOR_BRIGHT_MAGENTA  "\x1b[95m"
#define CC_COLOR_BRIGHT_CYAN     "\x1b[96m"
#define CC_COLOR_BRIGHT_WHITE    "\x1b[97m"
```

#### Settings Structs
```c
/* General OHLC settings for line and candle charts */
typedef struct cc_settings {
    const char* rise_color;    /* Rising segment/candle color (default: green) */
    const char* fall_color;    /* Falling segment/candle color (default: red) */
    const char* bg_color;      /* Background of empty cells (default: none) */
    const char* area_color;    /* Line fill color beneath curve (default: none) */
    int single_color;          /* 1 = single uniform color based on total change */
    int show_prices;           /* 1 = prepend 8-column price axis margin */
    int show_times;            /* 1 = append formatted timestamp footer */
} cc_settings_t;

/* Pie / Donut slice */
typedef struct cc_pie_slice {
    const char* label;
    double value;
} cc_pie_slice_t;

/* Pie / Donut settings */
typedef struct cc_pie_settings {
    const char* bg_color;
    const char* const* colors;
    int donut;
    int show_legend;
    int show_pct;
    double slice_gap;
    double inner_radius_ratio;
    int legend_format;
    double start_angle;
    int counter_clockwise;
    const char* center_text;
} cc_pie_settings_t;

/* Histogram settings */
typedef struct cc_hist_settings {
    const char* rise_color;
    const char* bg_color;
    int bin_count;
    double min_value;
    double max_value;
    int show_bins;
    int show_prices;
} cc_hist_settings_t;

/* Sparkline settings */
typedef struct cc_spark_settings {
    const char* rise_color;
    const char* area_color;
    int min_above;
    int min_below;
} cc_spark_settings_t;

/* Bar chart item */
typedef struct cc_bar_item {
    const char* label;
    double value;
} cc_bar_item_t;

/* Bar chart settings */
typedef struct cc_bar_settings {
    const char* rise_color;
    const char* bg_color;
    int show_labels;
    int show_prices;
} cc_bar_settings_t;

/* Stacked bar series */
typedef struct cc_stack_series {
    const char* name;
    const double* values;
} cc_stack_series_t;

/* Stacked bar settings */
typedef struct cc_stack_settings {
    const char* const* colors;
    const char* bg_color;
    const char* const* cat_labels;
    int series;
    int cats;
    int show_labels;
    int show_prices;
} cc_stack_settings_t;

/* Heatmap settings */
typedef struct cc_heat_settings {
    const char* low_color;
    const char* high_color;
    const char* mid_color;
    const char* bg_color;
    const char* const* row_labels;
    const char* const* col_labels;
    int show_labels;
} cc_heat_settings_t;

/* Box plot category */
typedef struct cc_box_category {
    const char* name;
    const double* samples;
    int n;
} cc_box_category_t;

/* Box plot settings */
typedef struct cc_box_settings {
    const char* rise_color;
    const char* area_color;
    const char* bg_color;
    int show_prices;
} cc_box_settings_t;
```

---

### Core Functions (`ccharts.h`)

```c
/* Ingestion & Parsing */
int cc_str_to_ohlc(const char* data, int size, cc_ohlc_t** ohlc, char val_seperator, char line_seperator);
int cc_json_to_ohlc(const char* json, cc_ohlc_t** ohlc, int* size);

/* Renderers (return malloc'd strings, owned by caller) */
char* cc_line_create(const cc_ohlc_t* data, int size, int width, int height, const cc_settings_t* settings);
char* cc_candle_create(const cc_ohlc_t* data, int size, int width, int height, const cc_settings_t* settings);
char* cc_pie_create(const cc_pie_slice_t* slices, int count, int width, int height, const cc_pie_settings_t* settings);
char* cc_hist_create(const double* samples, int count, int width, int height, const cc_hist_settings_t* settings);
char* cc_spark_create(const double* samples, int count, int width, int height, const cc_spark_settings_t* settings);
char* cc_bar_create(const cc_bar_item_t* items, int count, int width, int height, const cc_bar_settings_t* settings);
char* cc_stack_create(const cc_stack_series_t* series, int series_count, int width, int height, const cc_stack_settings_t* settings);
char* cc_heat_create(const double* values, int rows, int cols, int width, int height, const cc_heat_settings_t* settings);
char* cc_box_create(const cc_box_category_t* cats, int cat_count, int width, int height, const cc_box_settings_t* settings);
```

---

## 2. Flat C ABI (`abi/ccharts_abi.h`)

### Why the ABI Layer Exists
`ccharts.h` uses `static inline` functions and hides internal structs inside the `CCHARTS_IMPLEMENTATION` guard. Foreign function interfaces (FFI) in Rust, Go, Java FFM, C# P/Invoke, and WebAssembly cannot link against static inline functions.

`abi/ccharts_abi.c` compiles the implementation once and re-exports a stable `extern "C"` API with:
- **Opaque handle types** (`ccharts_data*`).
- **Explicit numeric status codes** (`ccharts_status`).
- **Standardized string lifetime** (`ccharts_string_free`).
- **Input boundary validation** (rejects non-finite floats, checks dimensions).

---

### Status Codes

```c
typedef enum ccharts_status {
    CCHARTS_OK = 0,
    CCHARTS_ERR_INVALID_ARG = 1,   /* NULL pointer, n <= 0, etc. */
    CCHARTS_ERR_PARSE = 2,         /* Malformed or empty JSON/CSV */
    CCHARTS_ERR_NOMEM = 3,         /* Memory allocation failed */
    CCHARTS_ERR_NON_FINITE = 4,    /* NaN or infinite value in inputs */
    CCHARTS_ERR_DIMENSIONS = 5     /* Dimensions <= 0 or exceeded CC_MAX_DIM / CC_MAX_CELLS */
} ccharts_status;
```

---

### ABI Export Surface

```c
/* 1. Dataset Construction */
CCHARTS_API int32_t ccharts_from_arrays(
    const double* open, const double* high, const double* low, const double* close,
    const int64_t* ts, int32_t n, ccharts_data** out
);
CCHARTS_API int32_t ccharts_parse_json(const char* json, ccharts_data** out);
CCHARTS_API int32_t ccharts_parse_csv(const char* csv, char value_separator, char line_separator, ccharts_data** out);
CCHARTS_API int32_t ccharts_data_len(const ccharts_data* data);
CCHARTS_API void    ccharts_data_free(ccharts_data* data);

/* 2. OHLC Rendering */
CCHARTS_API int32_t ccharts_line(const ccharts_data* data, int32_t width, int32_t height, const ccharts_settings* settings, char** out, size_t* out_len);
CCHARTS_API int32_t ccharts_candle(const ccharts_data* data, int32_t width, int32_t height, const ccharts_settings* settings, char** out, size_t* out_len);
CCHARTS_API void    ccharts_string_free(char* s);

/* 3. Standalone Renderers */
CCHARTS_API int32_t ccharts_pie_from_slices(
    const ccharts_pie_slice* slices, int32_t count, int32_t width, int32_t height,
    int32_t donut, const char* const* colors, int32_t color_count,
    int32_t show_legend, int32_t show_pct, double slice_gap,
    double inner_radius_ratio, int32_t legend_format, double start_angle,
    int32_t counter_clockwise, const char* center_text, char** out, size_t* out_len
);
CCHARTS_API int32_t ccharts_hist(const double* samples, int32_t count, int32_t width, int32_t height, const ccharts_hist_settings* settings, char** out, size_t* out_len);
CCHARTS_API int32_t ccharts_spark(const double* samples, int32_t count, int32_t width, int32_t height, const ccharts_spark_settings* settings, char** out, size_t* out_len);
CCHARTS_API int32_t ccharts_bar(const ccharts_bar_slice* items, int32_t count, int32_t width, int32_t height, const ccharts_bar_settings* settings, char** out, size_t* out_len);
CCHARTS_API int32_t ccharts_stack(const ccharts_stack_series* series, int32_t series_count, int32_t width, int32_t height, const ccharts_stack_settings* settings, char** out, size_t* out_len);
CCHARTS_API int32_t ccharts_heat(const double* values, int32_t rows, int32_t cols, int32_t width, int32_t height, const ccharts_heat_settings* settings, char** out, size_t* out_len);
CCHARTS_API int32_t ccharts_box(const ccharts_box_category* cats, int32_t cat_count, int32_t width, int32_t height, const ccharts_box_settings* settings, char** out, size_t* out_len);

/* 4. Introspection & Utilities */
CCHARTS_API const char* ccharts_color(int32_t index);
CCHARTS_API const char* ccharts_error_message(int32_t status);
CCHARTS_API const char* ccharts_version(void);
CCHARTS_API int32_t     ccharts_max_dim(void);
CCHARTS_API int32_t     ccharts_max_cells(void);
```

---

## 3. ABI Usage Example

```c
#include "ccharts_abi.h"
#include <stdio.h>

int main(void) {
    double opens[]  = { 100.0, 102.0, 101.0 };
    double highs[]  = { 103.0, 104.0, 105.0 };
    double lows[]   = { 99.0,  100.0, 100.5 };
    double closes[] = { 102.0, 101.0, 104.5 };
    
    ccharts_data* data = NULL;
    int32_t st = ccharts_from_arrays(opens, highs, lows, closes, NULL, 3, &data);
    if (st != CCHARTS_OK) {
        fprintf(stderr, "Error: %s\n", ccharts_error_message(st));
        return 1;
    }

    ccharts_settings s = {0};
    s.rise_color = ccharts_color(CCHARTS_COLOR_GREEN);
    s.fall_color = ccharts_color(CCHARTS_COLOR_RED);
    s.show_prices = 1;

    char* output = NULL;
    size_t len = 0;
    st = ccharts_candle(data, 60, 8, &s, &output, &len);
    if (st == CCHARTS_OK) {
        printf("%s\n", output);
        ccharts_string_free(output);
    }

    ccharts_data_free(data);
    return 0;
}
```

---

## 4. Platform & Compiler Compatibility

- **MSVC (Windows)**: C89-compliant, no variable-length arrays (VLAs), `CC_GMTIME_R` maps to `gmtime_s`.
- **GCC / Clang**: Built with `-std=c99 -pedantic -Wall -Wextra -Werror`.
- **Symbol Visibility**: Uses `__attribute__((visibility("default")))` and `__declspec(dllexport)` to export strictly the `ccharts_*` API surface.
