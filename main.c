/*
 * ccharts demo — renders the OHLC data from prices.txt as a line chart and
 * as candlestick charts (with and without axis labels).
 *
 * Build & run:
 *   make test && ./build/bin/test
 *
 * prices.txt is the JSON schema expected by cc_json_to_ohlc().
 */

#define CCHARTS_IMPLEMENTATION
#include "ccharts.h"

/* Reads a whole file into a malloc'd NUL-terminated buffer (caller frees). */
static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    size_t r = fread(buf, 1, (size_t)len, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

/* Forward declaration — render_pies is defined after main(). */
static void render_pies(void);

int main() {
    char* json = read_file("prices.txt");
    if (json == NULL) {
        printf("could not read prices.txt\n");
        return 1;
    }

    cc_ohlc_t* ohlc = NULL;
    int size = 0;
    if (cc_json_to_ohlc(json, &ohlc, &size) != 0 || size <= 0) {
        printf("JSON parse error\n");
        free(json);
        return 1;
    }
    free(json);

    int chart_width = 60;
    int chart_height = 8;

    cc_settings_t line_s = {
        .rise_color = CC_COLOR_BLUE,
        .fall_color = CC_COLOR_RED,
        .area_color = CC_COLOR_BRIGHT_BLACK,
        .show_prices = 1,
        .show_times = 1,
    };
    char* line = cc_line_create(ohlc, size, chart_width, chart_height, &line_s);
    if (line != NULL) {
        printf("LINE\n%s\n", line);
        free(line);
    }

    cc_settings_t candle_plain = {
        .rise_color = CC_COLOR_BLUE,
        .fall_color = CC_COLOR_RED,
        .bg_color = CC_COLOR_BRIGHT_BLACK,
    };
    char* candles = cc_candle_create(ohlc, size, chart_width, chart_height, &candle_plain);
    if (candles != NULL) {
        printf("CANDLE (no labels)\n%s\n", candles);
        free(candles);
    }

    cc_settings_t candle_s = candle_plain;
    candle_s.show_prices = 1;
    candle_s.show_times = 1;
    char* candles_lbl = cc_candle_create(ohlc, size, chart_width, chart_height, &candle_s);
    if (candles_lbl != NULL) {
        printf("CANDLE (labels)\n%s\n", candles_lbl);
        free(candles_lbl);
    }

    printf("\n");
    render_pies();

    free(ohlc);
    return 0;
}

/* Samples some pie/donut charts to eyeball the pie renderer. */
static void render_pies(void) {
    cc_pie_slice_t budget[] = {
        { .label = "Kira",   .value = 40.0 },
        { .label = "Gida",   .value = 25.0 },
        { .label = "Ulasim", .value = 15.0 },
        { .label = "Eglence", .value = 12.0 },
        { .label = "Diger",  .value = 8.0 },
    };

    cc_pie_settings_t disk = {
        .show_legend = 1,
        .show_pct    = 1,
        .start_angle = -1.0,   /* negative = unspecified -> CC_PI/2 (12 o'clock) */
        .inner_radius_ratio = -1.0,
    };
    char* d = cc_pie_create(budget, 5, 20, 8, &disk);
    if (d != NULL) {
        printf("PIE (disk, legend+pct)\n%s\n", d);
        free(d);
    }

    cc_pie_settings_t donut = {
        .donut       = 1,
        .show_legend = 1,
        .show_pct    = 1,
        .inner_radius_ratio = -1.0,   /* unspecified -> donut's 0.5 hollow */
        .start_angle = -1.0,
    };
    char* dn = cc_pie_create(budget, 5, 20, 12, &donut);
    if (dn != NULL) {
        printf("PIE (donut, legend+pct)\n%s\n", dn);
        free(dn);
    }

    cc_pie_settings_t override = {
        .donut       = 1,
        .colors      = (const char* const[]){ CC_COLOR_RED, CC_COLOR_BLUE,
                                              CC_COLOR_GREEN, CC_COLOR_YELLOW, NULL },
        .show_legend = 1,
        .show_pct    = 1,
        .inner_radius_ratio = -1.0,
        .start_angle = -1.0,
    };
    char* ov = cc_pie_create(budget, 5, 24, 10, &override);
    if (ov != NULL) {
        printf("PIE (donut, colors override)\n%s\n", ov);
        free(ov);
    }
}
