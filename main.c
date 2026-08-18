#define CCHARTS_IMPLEMENTATION
#include "ccharts.h"

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

    free(ohlc);
    return 0;
}
