/*
 * Links the static ABI library and exercises the whole surface once: build a
 * dataset three ways, render both chart types, and check that the error paths
 * report instead of silently producing an empty string. Run by `ctest`.
 */
#include <stdio.h>
#include <string.h>

#include "ccharts_abi.h"

static int failures = 0;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

int main(void) {
    static const double open[]  = {328.75, 330.0, 317.25, 320.0, 306.0};
    static const double high[]  = {330.0, 330.25, 321.0, 328.75, 307.25};
    static const double low[]   = {323.75, 317.5, 314.5, 317.75, 300.75};
    static const double close[] = {328.0, 317.5, 321.0, 318.0, 301.0};
    static const int64_t ts[]   = {1784505600, 1784592000, 1784678400,
                                   1784764800, 1784851200};
    static const char* json =
        "[{\"ts\":\"2026-07-20T00:00:00+00:00\",\"open\":328.75,\"high\":330.0,"
        "\"low\":323.75,\"close\":328.0}]";
    static const char* csv = "1,2,0.5,1.5\n\n  \n2,3,1,2.5\n";

    ccharts_data* data = NULL;
    ccharts_settings settings;
    char* chart = NULL;
    size_t len = 0;

    check(ccharts_from_arrays(open, high, low, close, ts, 5, &data) == CCHARTS_OK,
          "from_arrays");
    check(ccharts_data_len(data) == 5, "data_len");

    memset(&settings, 0, sizeof(settings));
    settings.rise_color = ccharts_color(CCHARTS_COLOR_BLUE);
    settings.show_prices = 1;
    settings.show_times = 1;

    check(ccharts_line(data, 40, 5, &settings, &chart, &len) == CCHARTS_OK, "line");
    check(chart != NULL && len == strlen(chart) && len > 0, "line output");
    printf("%s", chart);
    ccharts_string_free(chart);

    check(ccharts_candle(data, 40, 5, NULL, &chart, &len) == CCHARTS_OK, "candle");
    check(chart != NULL && len > 0, "candle output");
    ccharts_string_free(chart);

    /* Pie: no dataset, slices go straight to the renderer. */
    {
        ccharts_pie_slice slices[] = {{"Alpha", 40}, {"Beta", 30}, {"Gamma", 30}};
        const char* colors[] = {ccharts_color(CCHARTS_COLOR_RED),
                                ccharts_color(CCHARTS_COLOR_GREEN)};
        check(ccharts_pie_from_slices(slices, 3, 24, 10, 0, NULL, 0, 1, 1,
                                      0.0, -1.0, 0, -1.0, 0, NULL,
                                      &chart, &len) == CCHARTS_OK, "pie");
        check(chart != NULL && len > 0, "pie output");
        ccharts_string_free(chart);

        check(ccharts_pie_from_slices(slices, 3, 24, 10, 1, colors, 2, 1, 0,
                                      0.0, -1.0, 0, -1.0, 0, NULL,
                                      &chart, &len) == CCHARTS_OK, "donut pie");
        ccharts_string_free(chart);

        /* Every slice <= 0 renders the header's empty string, reported OK. */
        {
            ccharts_pie_slice zero[] = {{"Zero", 0}};
            check(ccharts_pie_from_slices(zero, 1, 24, 10, 0, NULL, 0, 1, 1,
                                          0.0, -1.0, 0, -1.0, 0, NULL,
                                          &chart, &len) == CCHARTS_OK, "all-zero pie");
            check(chart != NULL && chart[0] == '\0', "all-zero pie is empty");
            ccharts_string_free(chart);
        }

        /* NaN/inf must be rejected, like the price paths. */
        {
            ccharts_pie_slice bad[] = {{"Bad", 1.0}, {"Inf", 1.0}};
            bad[1].value = bad[0].value / 0.0;   /* +inf without <math.h> */
            check(ccharts_pie_from_slices(bad, 2, 24, 10, 0, NULL, 0, 1, 0,
                                          0.0, -1.0, 0, -1.0, 0, NULL,
                                          &chart, &len) == CCHARTS_ERR_NON_FINITE,
                  "inf pie rejected");
            check(chart == NULL, "no string on pie error");
        }
        check(ccharts_pie_from_slices(NULL, 0, 24, 10, 0, NULL, 0, 1, 0,
                                      0.0, -1.0, 0, -1.0, 0, NULL,
                                      &chart, &len) == CCHARTS_ERR_INVALID_ARG,
              "NULL pie slices rejected");
        check(ccharts_pie_from_slices(slices, 3, 0, 10, 0, NULL, 0, 1, 0,
                                      0.0, -1.0, 0, -1.0, 0, NULL,
                                      &chart, &len) == CCHARTS_ERR_DIMENSIONS,
              "zero width pie rejected");
    }

    /* Histogram: raw scalar samples, no dataset or capsule. */
    {
        static const double samples[] = {1,1,2,2,2,3,3,3,3,4,4,4,4,4,5,5,5,5,6,6,7};
        ccharts_hist_settings hs;
        memset(&hs, 0, sizeof(hs));
        check(ccharts_hist(samples, 21, 40, 5, &hs, &chart, &len) == CCHARTS_OK, "hist");
        check(chart != NULL && len > 0, "hist output");
        ccharts_string_free(chart);

        hs.bin_count = 5;
        hs.show_prices = 1;
        hs.show_bins = 1;
        check(ccharts_hist(samples, 21, 40, 5, &hs, &chart, &len) == CCHARTS_OK,
              "hist with settings");
        check(chart != NULL && len > 0, "hist settings output");
        ccharts_string_free(chart);

        check(ccharts_hist(NULL, 0, 40, 5, NULL, &chart, &len) == CCHARTS_ERR_INVALID_ARG,
              "NULL hist samples rejected");
        check(ccharts_hist(samples, 21, 0, 5, NULL, &chart, &len) == CCHARTS_ERR_DIMENSIONS,
              "zero width hist rejected");

        /* NaN/inf samples must be rejected at the boundary. */
        {
            double inf_s[2] = {1.0, 1.0};
            double nan_s[2] = {1.0, 0.0};
            static const double one = 1.0;
            inf_s[1] = one / 0.0;                       /* +inf without <math.h> */
            nan_s[1] = one / 0.0 - one / 0.0;           /* NaN */
            check(ccharts_hist(inf_s, 2, 40, 5, NULL, &chart, &len)
                  == CCHARTS_ERR_NON_FINITE, "inf hist sample rejected");
            check(ccharts_hist(nan_s, 2, 40, 5, NULL, &chart, &len)
                  == CCHARTS_ERR_NON_FINITE, "NaN hist sample rejected");
            check(chart == NULL, "no string on hist error");
        }
    }

    /* Sparkline: raw scalar samples, two heights, margins and error paths. */
    {
        static const double samples[] = {5,7,4,8,6,9,4,7,10,8,12,6,11,9,13};
        ccharts_spark_settings ss;
        memset(&ss, 0, sizeof(ss));
        check(ccharts_spark(samples, 15, 24, 1, &ss, &chart, &len) == CCHARTS_OK,
              "spark");
        check(chart != NULL && len > 0, "spark output");
        ccharts_string_free(chart);

        ss.rise_color = ccharts_color(CCHARTS_COLOR_BLUE);
        ss.area_color = ccharts_color(CCHARTS_COLOR_BRIGHT_BLACK);
        ss.min_above = 2;
        ss.min_below = 1;
        check(ccharts_spark(samples, 15, 24, 2, &ss, &chart, &len) == CCHARTS_OK,
              "spark with settings");
        check(chart != NULL && len > 0, "spark settings output");
        ccharts_string_free(chart);

        check(ccharts_spark(NULL, 0, 24, 1, NULL, &chart, &len)
              == CCHARTS_ERR_INVALID_ARG, "NULL spark samples rejected");
        check(ccharts_spark(samples, 15, 0, 1, NULL, &chart, &len)
              == CCHARTS_ERR_DIMENSIONS, "zero width spark rejected");

        /* NaN/inf samples must be rejected at the boundary. */
        {
            double inf_s[2] = {1.0, 1.0};
            double nan_s[2] = {1.0, 0.0};
            static const double one = 1.0;
            inf_s[1] = one / 0.0;                       /* +inf without <math.h> */
            nan_s[1] = one / 0.0 - one / 0.0;           /* NaN */
            check(ccharts_spark(inf_s, 2, 24, 1, NULL, &chart, &len)
                  == CCHARTS_ERR_NON_FINITE, "inf spark sample rejected");
            check(ccharts_spark(nan_s, 2, 24, 1, NULL, &chart, &len)
                  == CCHARTS_ERR_NON_FINITE, "NaN spark sample rejected");
            check(chart == NULL, "no string on spark error");
        }
    }

    /* Invalid dimensions must be reported, not answered with "". */
    check(ccharts_line(data, 0, 5, NULL, &chart, &len) == CCHARTS_ERR_DIMENSIONS,
          "zero width rejected");
    check(chart == NULL, "no string on error");
    check(ccharts_line(data, 1000, 2000, NULL, &chart, &len) == CCHARTS_ERR_DIMENSIONS,
          "over CC_MAX_CELLS rejected");
    ccharts_data_free(data);

    data = NULL;
    check(ccharts_parse_json(json, &data) == CCHARTS_OK, "parse_json");
    check(ccharts_data_len(data) == 1, "parse_json len");
    ccharts_data_free(data);

    data = NULL;
    check(ccharts_parse_json("not json", &data) == CCHARTS_ERR_PARSE, "bad json");
    check(data == NULL, "no handle on parse error");

    /* Blank lines are skipped by the parser, so the dataset must hold 2 rows
     * rather than the 4 physical lines. */
    data = NULL;
    check(ccharts_parse_csv(csv, ',', '\n', &data) == CCHARTS_OK, "parse_csv");
    check(ccharts_data_len(data) == 2, "parse_csv skips blank lines");
    ccharts_data_free(data);

    {
        double bad[] = {1.0};
        double nan_val[] = {0.0};
        nan_val[0] = bad[0] / 0.0 - bad[0] / 0.0;   /* NaN without <math.h> */
        data = NULL;
        check(ccharts_from_arrays(nan_val, bad, bad, bad, NULL, 1, &data)
              == CCHARTS_ERR_NON_FINITE, "NaN rejected");
    }

    check(ccharts_color(CCHARTS_COLOR_RESET) != NULL, "color table");
    check(ccharts_color(-1) == NULL && ccharts_color(999) == NULL, "color range");
    check(ccharts_max_dim() == 100000 && ccharts_max_cells() == 1000000, "limits");
    check(strcmp(ccharts_version(), CCHARTS_VERSION) == 0, "version");

    if (failures == 0) {
        printf("smoke: all checks passed\n");
        return 0;
    }
    printf("smoke: %d check(s) failed\n", failures);
    return 1;
}
