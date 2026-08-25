/*
 * ccharts_core.c — Lua C module for the ccharts charting library.
 *
 * This module compiles against the vendored C ABI (bindings/lua/vendor/) and
 * exposes a low-level FFI surface that the pure-Lua facade (src/ccharts.lua)
 * calls into. It owns all the marshalling between Lua tables and the C ABI:
 *
 *   - Lua tables  -> ccharts_*_settings structs (pointers stay valid because
 *                    the settings table remains referenced on the Lua stack
 *                    for the whole render call),
 *   - Lua arrays  -> contiguous, malloc'd blocks of structs (pie / bar /
 *                    stack / box all build ONE contiguous block of
 *                    size*count and place each element at its byte offset —
 *                    never an array of pointers).
 *
 * Every renderer returns the C string by value (copied into a Lua string, the
 * C buffer released with ccharts_string_free immediately after), and raises a
 * Lua error on any non-CCHARTS_OK status.
 *
 * Data handles are wrapped in a full userdata whose __gc releases them with
 * ccharts_data_free, so ownership is handled automatically.
 */

#include <lua.h>
#include <lauxlib.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#if defined(NAN)
#define CC_NAN ((double)NAN)
#elif defined(_MSC_VER)
#define CC_NAN ((double)sqrt(-1.0))
#else
#define CC_NAN (0.0 / 0.0)
#endif

#include "ccharts_abi.h"

#define DATA_MT "ccharts.data"

typedef struct { ccharts_data* h; } ccharts_data_ud;

/* ----------------------------- helpers ----------------------------- */

/* Capture the absolute index of the i-th arg. Call at function entry, before
 * any pushes, so it survives later temporary push/pop cycles. */
#define ABSIDX(L, i) lua_absindex((L), (i))

static int opt_bool(lua_State* L, int t, const char* key, int def) {
    int v = def;
    lua_getfield(L, t, key);
    if (!lua_isnil(L, -1)) v = lua_toboolean(L, -1) ? 1 : 0;
    lua_pop(L, 1);
    return v;
}

static int opt_int(lua_State* L, int t, const char* key, int def) {
    int v = def;
    lua_getfield(L, t, key);
    if (!lua_isnil(L, -1)) v = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return v;
}

static double opt_double(lua_State* L, int t, const char* key, double def) {
    double v = def;
    lua_getfield(L, t, key);
    if (!lua_isnil(L, -1)) v = (double)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return v;
}

/* Read a string field into *out as NULL when absent. The returned pointer is
 * valid while the settings table t stays on the Lua stack (it does for the
 * whole call). */
static void opt_cstr(lua_State* L, int t, const char* key, const char** out) {
    lua_getfield(L, t, key);
    *out = lua_isnil(L, -1) ? NULL : lua_tostring(L, -1);
    lua_pop(L, 1);
}

/* Read the array-of-numbers table at index arr into a freshly malloc'd
 * double* of length *out_n. Caller frees. */
static double* read_doubles(lua_State* L, int arr, int* out_n) {
    lua_len(L, arr);
    lua_Integer n = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (n < 0) n = 0;
    double* d = (double*)malloc((size_t)(n > 0 ? n : 1) * sizeof(double));
    if (!d) luaL_error(L, "out of memory");
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, arr, (lua_Integer)i);
        d[i - 1] = (double)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
    *out_n = (int)n;
    return d;
}

static int64_t* read_i64s(lua_State* L, int arr, int* out_n) {
    lua_len(L, arr);
    lua_Integer n = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (n < 0) n = 0;
    int64_t* d = (int64_t*)malloc((size_t)(n > 0 ? n : 1) * sizeof(int64_t));
    if (!d) luaL_error(L, "out of memory");
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, arr, (lua_Integer)i);
        d[i - 1] = (int64_t)luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }
    *out_n = (int)n;
    return d;
}

/* Read a settings-table field that is an array of strings into a malloc'd
 * array of const char*. Returns count via *out_n (0 and NULL when the field
 * is absent) and the array (caller frees). Strings stay valid because the
 * settings table t (which references this array) stays on the stack for the
 * whole call. */
static const char** read_str_array(lua_State* L, int t, const char* key,
                                   int* out_n) {
    lua_getfield(L, t, key);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        *out_n = 0;
        return NULL;
    }
    int arr = lua_absindex(L, -1);
    lua_len(L, arr);
    lua_Integer n = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (n < 0) n = 0;
    const char** a = (const char**)malloc(
        (size_t)(n > 0 ? n : 1) * sizeof(char*));
    if (!a) luaL_error(L, "out of memory");
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, arr, (lua_Integer)i);
        a[i - 1] = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    lua_pop(L, 1); /* pop the field */
    *out_n = (int)n;
    return a;
}

/* Wrap a raw ccharts_data* in a userdata with a __gc finalizer. */
static int data_gc(lua_State* L) {
    ccharts_data_ud* ud = (ccharts_data_ud*)luaL_checkudata(L, 1, DATA_MT);
    if (ud->h) { ccharts_data_free(ud->h); ud->h = NULL; }
    return 0;
}

static int push_data(lua_State* L, ccharts_data* h) {
    ccharts_data_ud* ud = (ccharts_data_ud*)lua_newuserdatauv(L, sizeof(*ud), 0);
    ud->h = h;
    luaL_getmetatable(L, DATA_MT);
    lua_setmetatable(L, -2);
    return 1;
}

/* Push the render result as a Lua string (copying, then freeing the C
 * buffer), or raise a Lua error on a non-OK status. */
static int push_string(lua_State* L, int status, char* out, size_t out_len) {
    if (status != CCHARTS_OK) {
        if (out) ccharts_string_free(out);
        return luaL_error(L, "%s", ccharts_error_message(status));
    }
    lua_pushlstring(L, out ? out : "", out ? out_len : 0);
    ccharts_string_free(out);
    return 1;
}

/* ------------------------- data construction ------------------------ */

static int l_from_arrays(lua_State* L) {
    int o_i = ABSIDX(L, 1), h_i = ABSIDX(L, 2),
        l_i = ABSIDX(L, 3), c_i = ABSIDX(L, 4);
    int no, nh, nl, nc;
    double* o = read_doubles(L, o_i, &no);
    double* h = read_doubles(L, h_i, &nh);
    double* lo = read_doubles(L, l_i, &nl);
    double* c = read_doubles(L, c_i, &nc);
    if (no != nh || no != nl || no != nc) {
        free(o); free(h); free(lo); free(c);
        return luaL_error(L, "open, high, low and close must have the same length");
    }
    int64_t* ts = NULL;
    int nts = 0;
    if (lua_gettop(L) >= 5 && !lua_isnil(L, 5))
        ts = read_i64s(L, ABSIDX(L, 5), &nts);

    ccharts_data* out = NULL;
    int status = ccharts_from_arrays(o, h, lo, c, ts, no, &out);
    free(o); free(h); free(lo); free(c); free(ts);
    if (status != CCHARTS_OK)
        return luaL_error(L, "%s", ccharts_error_message(status));
    return push_data(L, out);
}

static int l_from_json(lua_State* L) {
    const char* json = luaL_checkstring(L, 1);
    ccharts_data* out = NULL;
    int status = ccharts_parse_json(json, &out);
    if (status != CCHARTS_OK)
        return luaL_error(L, "%s", ccharts_error_message(status));
    return push_data(L, out);
}

static int l_from_csv(lua_State* L) {
    const char* csv = luaL_checkstring(L, 1);
    char vs = (char)luaL_checkinteger(L, 2);
    char ls = (char)luaL_checkinteger(L, 3);
    ccharts_data* out = NULL;
    int status = ccharts_parse_csv(csv, vs, ls, &out);
    if (status != CCHARTS_OK)
        return luaL_error(L, "%s", ccharts_error_message(status));
    return push_data(L, out);
}

static int l_data_len(lua_State* L) {
    ccharts_data_ud* ud = (ccharts_data_ud*)luaL_checkudata(L, 1, DATA_MT);
    lua_pushinteger(L, ccharts_data_len(ud->h));
    return 1;
}

static int l_data_free(lua_State* L) {
    ccharts_data_ud* ud = (ccharts_data_ud*)luaL_checkudata(L, 1, DATA_MT);
    if (ud->h) { ccharts_data_free(ud->h); ud->h = NULL; }
    return 0;
}

/* -------------------------- line / candle -------------------------- */

static int l_line_candle(lua_State* L, int candle) {
    ccharts_data_ud* ud = (ccharts_data_ud*)luaL_checkudata(L, 1, DATA_MT);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);
    ccharts_settings s;
    memset(&s, 0, sizeof(s));
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) {
        int t = ABSIDX(L, 4);
        opt_cstr(L, t, "rise_color", &s.rise_color);
        opt_cstr(L, t, "fall_color", &s.fall_color);
        opt_cstr(L, t, "bg_color", &s.bg_color);
        opt_cstr(L, t, "area_color", &s.area_color);
        s.single_color = opt_bool(L, t, "single_color", 0);
        s.show_prices = opt_bool(L, t, "show_prices", 0);
        s.show_times = opt_bool(L, t, "show_times", 0);
    }
    char* out = NULL; size_t out_len = 0;
    int status = (candle ? ccharts_candle : ccharts_line)(
        ud->h, width, height, &s, &out, &out_len);
    return push_string(L, status, out, out_len);
}

static int l_line(lua_State* L) { return l_line_candle(L, 0); }
static int l_candle(lua_State* L) { return l_line_candle(L, 1); }

/* ------------------------------- pie ------------------------------- */

static int l_pie(lua_State* L) {
    int slices_i = ABSIDX(L, 1);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);

    lua_len(L, slices_i);
    lua_Integer n = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (n <= 0)
        return luaL_error(L, CCHARTS_ERR_INVALID_ARG == CCHARTS_ERR_INVALID_ARG
                            ? "need at least one slice"
                            : ccharts_error_message(CCHARTS_ERR_INVALID_ARG));

    /* Contiguous block of ccharts_pie_slice (label*, double) = 16 B each. */
    ccharts_pie_slice* slices = (ccharts_pie_slice*)malloc(
        (size_t)n * sizeof(ccharts_pie_slice));
    if (!slices) return luaL_error(L, "out of memory");
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, slices_i, (lua_Integer)i);
        int sl = lua_absindex(L, -1);
        lua_getfield(L, sl, "label");
        slices[i - 1].label = lua_isnil(L, -1) ? NULL : lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, sl, "value");
        slices[i - 1].value = lua_isnil(L, -1) ? 0.0 : (double)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_pop(L, 1); /* pop the slice table */
    }

    int t = 0;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) t = ABSIDX(L, 4);

    int donut = t ? opt_bool(L, t, "donut", 0) : 0;
    int show_legend = t ? opt_bool(L, t, "show_legend", 1) : 1;
    int show_pct = t ? opt_bool(L, t, "show_pct", 0) : 0;
    double slice_gap = t ? opt_double(L, t, "slice_gap", 0.0) : 0.0;
    double inner_radius_ratio = t ? opt_double(L, t, "inner_radius_ratio", -1.0) : -1.0;
    int legend_format = t ? opt_int(L, t, "legend_format", 0) : 0;
    double start_angle = t ? opt_double(L, t, "start_angle", -1.0) : -1.0;
    int counter_clockwise = t ? opt_bool(L, t, "counter_clockwise", 0) : 0;
    const char* center_text = NULL;
    if (t) opt_cstr(L, t, "center_text", &center_text);

    const char** colors = NULL;
    int color_count = 0;
    if (t) colors = read_str_array(L, t, "colors", &color_count);

    char* out = NULL; size_t out_len = 0;
    int status = ccharts_pie_from_slices(
        slices, (int)n, width, height, donut, (const char* const*)colors,
        color_count,
        show_legend, show_pct, slice_gap, inner_radius_ratio, legend_format,
        start_angle, counter_clockwise, center_text, &out, &out_len);
    free(slices); free(colors);
    return push_string(L, status, out, out_len);
}

/* ---------------------------- hist / spark --------------------------- */

static int l_hist(lua_State* L) {
    int samples_i = ABSIDX(L, 1);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);
    int n;
    double* samples = read_doubles(L, samples_i, &n);
    if (n <= 0) { free(samples); return luaL_error(L, "need at least one sample"); }

    ccharts_hist_settings s;
    memset(&s, 0, sizeof(s));
    int t = 0;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) t = ABSIDX(L, 4);
    if (t) {
        opt_cstr(L, t, "rise_color", &s.rise_color);
        opt_cstr(L, t, "bg_color", &s.bg_color);
        s.bin_count = opt_int(L, t, "bin_count", 0);
        s.min_value = opt_double(L, t, "min_value", CC_NAN);
        s.max_value = opt_double(L, t, "max_value", CC_NAN);
        s.show_bins = opt_bool(L, t, "show_bins", 0);
        s.show_prices = opt_bool(L, t, "show_prices", 0);
    }

    char* out = NULL; size_t out_len = 0;
    int status = ccharts_hist(samples, n, width, height, &s, &out, &out_len);
    free(samples);
    return push_string(L, status, out, out_len);
}

static int l_spark(lua_State* L) {
    int samples_i = ABSIDX(L, 1);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);
    int n;
    double* samples = read_doubles(L, samples_i, &n);
    if (n <= 0) { free(samples); return luaL_error(L, "need at least one sample"); }

    ccharts_spark_settings s;
    memset(&s, 0, sizeof(s));
    int t = 0;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) t = ABSIDX(L, 4);
    if (t) {
        opt_cstr(L, t, "rise_color", &s.rise_color);
        opt_cstr(L, t, "area_color", &s.area_color);
        s.min_above = opt_int(L, t, "min_above", 0);
        s.min_below = opt_int(L, t, "min_below", 0);
    }

    char* out = NULL; size_t out_len = 0;
    int status = ccharts_spark(samples, n, width, height, &s, &out, &out_len);
    free(samples);
    return push_string(L, status, out, out_len);
}

/* ------------------------------- bar -------------------------------- */

static int l_bar(lua_State* L) {
    int items_i = ABSIDX(L, 1);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);

    lua_len(L, items_i);
    lua_Integer n = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (n <= 0) return luaL_error(L, "need at least one bar");

    /* Contiguous block of ccharts_bar_slice (label*, double) = 16 B each. */
    ccharts_bar_slice* items = (ccharts_bar_slice*)malloc(
        (size_t)n * sizeof(ccharts_bar_slice));
    if (!items) return luaL_error(L, "out of memory");
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, items_i, (lua_Integer)i);
        int it = lua_absindex(L, -1);
        lua_getfield(L, it, "label");
        items[i - 1].label = lua_isnil(L, -1) ? NULL : lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, it, "value");
        items[i - 1].value = lua_isnil(L, -1) ? 0.0 : (double)lua_tonumber(L, -1);
        lua_pop(L, 1);
        lua_pop(L, 1);
    }

    ccharts_bar_settings s;
    memset(&s, 0, sizeof(s));
    int t = 0;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) t = ABSIDX(L, 4);
    if (t) {
        opt_cstr(L, t, "rise_color", &s.rise_color);
        opt_cstr(L, t, "bg_color", &s.bg_color);
        s.show_labels = opt_bool(L, t, "show_labels", 0);
        s.show_prices = opt_bool(L, t, "show_prices", 0);
    }

    char* out = NULL; size_t out_len = 0;
    int status = ccharts_bar(items, (int)n, width, height, &s, &out, &out_len);
    free(items);
    return push_string(L, status, out, out_len);
}

/* --------------------------- stacked bar ---------------------------- */

static int l_stack(lua_State* L) {
    int series_i = ABSIDX(L, 1);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);

    lua_len(L, series_i);
    lua_Integer n = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (n <= 0) return luaL_error(L, "need at least one series");

    /* Contiguous block of ccharts_stack_series (name*, values*) = 16 B each. */
    ccharts_stack_series* series = (ccharts_stack_series*)malloc(
        (size_t)n * sizeof(ccharts_stack_series));
    if (!series) return luaL_error(L, "out of memory");
    int cats = -1;
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, series_i, (lua_Integer)i);
        int sr = lua_absindex(L, -1);
        lua_getfield(L, sr, "name");
        series[i - 1].name = lua_isnil(L, -1) ? NULL : lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, sr, "values");
        if (lua_isnil(L, -1)) {
            series[i - 1].values = NULL;
            lua_pop(L, 1);
        } else {
            int varr = lua_absindex(L, -1);
            int vn;
            series[i - 1].values = read_doubles(L, varr, &vn);
            if (cats < 0) cats = vn;
            else if (vn != cats) return luaL_error(L, "all series must have the same number of values");
            lua_pop(L, 1); /* pop values field */
        }
        lua_pop(L, 1); /* pop the series table */
    }

    ccharts_stack_settings s;
    memset(&s, 0, sizeof(s));
    int t = 0;
    int nc = 0, cc = 0;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) t = ABSIDX(L, 4);
    if (t) {
        /* cat_labels: array of `cats` non-NULL-terminated strings. */
        s.cat_labels = (const char* const*)read_str_array(L, t, "cat_labels", &nc);
        opt_cstr(L, t, "bg_color", &s.bg_color);
        s.show_labels = opt_bool(L, t, "show_labels", 0);
        s.show_prices = opt_bool(L, t, "show_prices", 0);
        /* colors: NULL-terminated palette override, or NULL for default. */
        const char** col = read_str_array(L, t, "colors", &cc);
        if (col) {
            const char** nullterm = (const char**)malloc(
                (size_t)(cc + 1) * sizeof(char*));
            memcpy(nullterm, col, (size_t)cc * sizeof(char*));
            nullterm[cc] = NULL;
            free(col);
            s.colors = (const char* const*)nullterm;
        } else {
            s.colors = NULL;
        }
    }
    if (cats < 0) cats = 0;
    s.series = (int)n;
    s.cats = cats;

    char* out = NULL; size_t out_len = 0;
    int status = ccharts_stack(series, (int)n, width, height, &s, &out, &out_len);
    for (lua_Integer i = 0; i < n; i++) free((void*)series[i].values);
    free(series);
    free((void*)s.colors);
    free((void*)s.cat_labels);
    return push_string(L, status, out, out_len);
}

/* ------------------------------ heatmap ----------------------------- */

static int l_heat(lua_State* L) {
    int values_i = ABSIDX(L, 1);
    int rows = (int)luaL_checkinteger(L, 2);
    int cols = (int)luaL_checkinteger(L, 3);
    int width = (int)luaL_checkinteger(L, 4);
    int height = (int)luaL_checkinteger(L, 5);

    double* flat = (double*)malloc((size_t)(rows * cols) * sizeof(double));
    if (!flat) return luaL_error(L, "out of memory");
    for (int r = 0; r < rows; r++) {
        lua_rawgeti(L, values_i, (lua_Integer)(r + 1));
        int row = lua_absindex(L, -1);
        for (int c = 0; c < cols; c++) {
            lua_rawgeti(L, row, (lua_Integer)(c + 1));
            flat[(size_t)r * cols + c] = (double)luaL_checknumber(L, -1);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    ccharts_heat_settings s;
    memset(&s, 0, sizeof(s));
    int t = 0;
    int nrl = 0, ncl = 0;
    if (lua_gettop(L) >= 6 && !lua_isnil(L, 6)) t = ABSIDX(L, 6);
    if (t) {
        opt_cstr(L, t, "low_color", &s.low_color);
        opt_cstr(L, t, "high_color", &s.high_color);
        opt_cstr(L, t, "mid_color", &s.mid_color);
        opt_cstr(L, t, "bg_color", &s.bg_color);
        s.row_labels = (const char* const*)read_str_array(L, t, "row_labels", &nrl);
        s.col_labels = (const char* const*)read_str_array(L, t, "col_labels", &ncl);
        s.show_labels = opt_bool(L, t, "show_labels", 0);
    }

    char* out = NULL; size_t out_len = 0;
    int status = ccharts_heat(flat, rows, cols, width, height, &s, &out, &out_len);
    free(flat);
    free((void*)s.row_labels);
    free((void*)s.col_labels);
    return push_string(L, status, out, out_len);
}

/* ------------------------------ box plot ---------------------------- */

static int l_box(lua_State* L) {
    int cats_i = ABSIDX(L, 1);
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);

    lua_len(L, cats_i);
    lua_Integer n = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    if (n <= 0) return luaL_error(L, "need at least one category");

    /* Contiguous block of ccharts_box_category (name*, samples*, int32 n),
     * padded by C to 24 B per element. */
    ccharts_box_category* cats = (ccharts_box_category*)malloc(
        (size_t)n * sizeof(ccharts_box_category));
    if (!cats) return luaL_error(L, "out of memory");
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, cats_i, (lua_Integer)i);
        int ct = lua_absindex(L, -1);
        lua_getfield(L, ct, "name");
        cats[i - 1].name = lua_isnil(L, -1) ? NULL : lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, ct, "samples");
        int vn = 0;
        if (lua_isnil(L, -1)) {
            cats[i - 1].samples = NULL;
            cats[i - 1].n = 0;
            lua_pop(L, 1);
        } else {
            int varr = lua_absindex(L, -1);
            cats[i - 1].samples = read_doubles(L, varr, &vn);
            cats[i - 1].n = vn;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    ccharts_box_settings s;
    memset(&s, 0, sizeof(s));
    int t = 0;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) t = ABSIDX(L, 4);
    if (t) {
        opt_cstr(L, t, "rise_color", &s.rise_color);
        opt_cstr(L, t, "area_color", &s.area_color);
        opt_cstr(L, t, "bg_color", &s.bg_color);
        s.show_prices = opt_bool(L, t, "show_prices", 0);
    }

    char* out = NULL; size_t out_len = 0;
    int status = ccharts_box(cats, (int)n, width, height, &s, &out, &out_len);
    for (lua_Integer i = 0; i < n; i++) free((void*)cats[i].samples);
    free(cats);
    return push_string(L, status, out, out_len);
}

/* --------------------------- introspection --------------------------- */

static int l_version(lua_State* L) {
    lua_pushstring(L, ccharts_version());
    return 1;
}
static int l_max_dim(lua_State* L) {
    lua_pushinteger(L, ccharts_max_dim());
    return 1;
}
static int l_max_cells(lua_State* L) {
    lua_pushinteger(L, ccharts_max_cells());
    return 1;
}
static int l_color(lua_State* L) {
    const char* c = ccharts_color((int)luaL_checkinteger(L, 1));
    if (c) lua_pushstring(L, c);
    else lua_pushnil(L);
    return 1;
}
static int l_error_message(lua_State* L) {
    lua_pushstring(L, ccharts_error_message((int)luaL_checkinteger(L, 1)));
    return 1;
}

/* ------------------------------ module ------------------------------ */

static const luaL_Reg datam[] = {
    { "size", l_data_len },
    { "len", l_data_len },
    { "free", l_data_free },
    { "__gc", data_gc },
    { NULL, NULL },
};

static const luaL_Reg ccharts_core[] = {
    { "_version", l_version },
    { "_max_dim", l_max_dim },
    { "_max_cells", l_max_cells },
    { "_color", l_color },
    { "_error_message", l_error_message },
    { "_from_arrays", l_from_arrays },
    { "_from_json", l_from_json },
    { "_from_csv", l_from_csv },
    { "_data_len", l_data_len },
    { "_line", l_line },
    { "_candle", l_candle },
    { "_pie", l_pie },
    { "_hist", l_hist },
    { "_spark", l_spark },
    { "_bar", l_bar },
    { "_stack", l_stack },
    { "_heat", l_heat },
    { "_box", l_box },
    /* Friendly aliases so `require "ccharts"; c.version` also works when the
     * C module itself is loaded directly (the top-level dev build). */
    { "color", l_color },
    { "error_message", l_error_message },
    { NULL, NULL },
};

static int create_module(lua_State* L) {
    luaL_newmetatable(L, DATA_MT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, datam, 0);
    lua_pop(L, 1);

    luaL_newlib(L, ccharts_core);
    /* version / max_dim / max_cells as plain values (mirrors the facade). */
    lua_pushstring(L, ccharts_version());   lua_setfield(L, -2, "version");
    lua_pushinteger(L, ccharts_max_dim());  lua_setfield(L, -2, "max_dim");
    lua_pushinteger(L, ccharts_max_cells());lua_setfield(L, -2, "max_cells");
    return 1;
}

/* The module is exported under both names: `ccharts_core` for the pure-Lua
 * facade (src/ccharts.lua requires it), and `ccharts` so the compiled module
 * can be loaded directly as the top-level `require "ccharts"` (the dev gate).
 * The rockspec installs the facade as `ccharts` and this C module as
 * `ccharts_core`; both luaopen_* symbols live in the same object file. */
int luaopen_ccharts(lua_State* L) { return create_module(L); }
int luaopen_ccharts_core(lua_State* L) { return create_module(L); }
