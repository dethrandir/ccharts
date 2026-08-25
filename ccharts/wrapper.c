/*
 * ccharts._core — CPython extension wrapping ccharts.h.
 *
 * The module exposes three low-level functions; the high-level `Chart`
 * class lives in ccharts/__init__.py:
 *
 *   parse_json(json: str) -> PyCapsule
 *       Parses the fixed-schema JSON into cc_ohlc_t via cc_json_to_ohlc()
 *       and wraps the result in a PyCapsule. The capsule owns the OHLC
 *       buffer and frees it when garbage collected.
 *
 *   parse_arrays(open, high, low, close, ts=None) -> PyCapsule
 *       Builds the same capsule from four equal-length columns instead of a
 *       JSON document. Each column may be any float64 C-contiguous buffer
 *       (numpy array, array.array, memoryview) or any Python sequence; `ts`
 *       is epoch seconds (int64 buffer or sequence) and defaults to 0. This
 *       is the path used by Chart.from_dataframe / Chart.from_arrays, so
 *       DataFrames never have to be serialized to JSON first.
 *
 *   create_line(capsule, width, height, rise, fall, bg, area, single, prices, times)
 *   create_candle(...)                                     -> str
 *       Renders a chart from the capsule's data and returns the ANSI-colored
 *       string. Every settings flag maps 1:1 to cc_settings_t fields.
 *       Raises ValueError for non-positive or over-limit dimensions
 *       (CC_MAX_DIM / CC_MAX_CELLS from ccharts.h) and for invalid capsules.
 *
 *   create_pie(labels, values, width, height, donut, colors, bg, legend, pct) -> str
 *       Renders a pie/donut from parallel label/value slices (no capsule
 *       needed — a pie is not OHLC data). Raises ValueError for non-finite
 *       values (NaN/inf), invalid dimensions, mismatched lengths, and
 *       non-str/non-number items; slice values <= 0 make the renderer return
 *       the empty string, exactly like cc_pie_create.
 *
 *   create_hist(samples, width, height, rise, bg, bins, min, max, bins_flag, prices) -> str
 *   create_spark(samples, width, height, rise, area, min_above, min_below) -> str
 *       Both render a 1-D sequence of scalar samples (no capsule). The
 *       histogram draws vertical bars into equal-width bins; the sparkline
 *       draws a tiny axis-less trend line. Both raise ValueError for
 *       non-finite samples (NaN/inf) and invalid dimensions; empty samples
 *       render the empty string, exactly like cc_hist_create / cc_spark_create.
 *
 * Build: `make test-py` (see setup.py).
 */

#define PY_SSIZE_T_CLEAN
#define CCHARTS_IMPLEMENTATION
#include <Python.h>
#include <limits.h>
#include "../ccharts.h"

/* Python-side handle to a parsed OHLC array. */
typedef struct {
    cc_ohlc_t* data;
    int size;
} py_ohlc_data;

/* PyCapsule destructor: frees the C memory when Python collects the capsule. */
static void dealloc_ohlc(PyObject* capsule) {
    if (PyCapsule_IsValid(capsule, "ccharts.ohlc")) {
        py_ohlc_data* od = (py_ohlc_data*)PyCapsule_GetPointer(capsule, "ccharts.ohlc");
        if (od != NULL) {
            free(od->data);
            free(od);
        }
    }
}

/* Validates chart dimensions against the C library limits. */
static int check_dimensions(int width, int height) {
    return width > 0 && height > 0 &&
           width <= CC_MAX_DIM && height <= CC_MAX_DIM &&
           (long long)width * (long long)height <= CC_MAX_CELLS;
}

/* Same finite guard as check_finite, but over JSON-parsed cc_ohlc_t rows:
 * the JSON scanner accepts "1e999"-style overflows (atof gives inf), which
 * must never reach the renderers' lround(). */
static int check_ohlc_finite(const cc_ohlc_t* data, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (!isfinite(data[i].open) || !isfinite(data[i].high) ||
            !isfinite(data[i].low) || !isfinite(data[i].close)) {
            PyErr_SetString(PyExc_ValueError,
                            "OHLC values must be finite (no NaN or inf)");
            return -1;
        }
    }
    return 0;
}

/* Parses a JSON string and returns a PyCapsule holding the cc_ohlc_t array. */
static PyObject* py_parse_json(PyObject* self, PyObject* args) {
    const char* json;
    if (!PyArg_ParseTuple(args, "s", &json)) return NULL;

    cc_ohlc_t* ohlc = NULL;
    int size = 0;
    if (cc_json_to_ohlc(json, &ohlc, &size) != 0 || size <= 0) {
        PyErr_SetString(PyExc_ValueError, "failed to parse JSON");
        return NULL;
    }

    if (check_ohlc_finite(ohlc, size) != 0) {
        free(ohlc);
        return NULL;
    }

    py_ohlc_data* od = (py_ohlc_data*)malloc(sizeof(py_ohlc_data));
    if (od == NULL) {
        free(ohlc);
        PyErr_NoMemory();
        return NULL;
    }
    od->data = ohlc;
    od->size = size;

    PyObject* capsule = PyCapsule_New(od, "ccharts.ohlc", &dealloc_ohlc);
    if (capsule == NULL) {
        /* The capsule was never created, so its destructor never runs:
         * release the OHLC array and the handle the way dealloc_ohlc would. */
        free(od->data);
        free(od);
        PyErr_NoMemory();
        return NULL;
    }
    return capsule;
}

/* ========================== Array input path ==========================
 * Columnar input (four price columns + optional timestamps) instead of a
 * JSON document. Each column is read through the buffer protocol when it
 * exposes a 1-D C-contiguous float64/int64 block (numpy, array.array), and
 * falls back to the generic sequence protocol otherwise (list, tuple, or a
 * buffer with a dtype we do not memcpy directly). Both paths end in the
 * same cc_ohlc_t array and the same "ccharts.ohlc" capsule as parse_json.
 * ====================================================================== */

/* Length of a column argument, or -1 with an exception set. */
static Py_ssize_t arg_len(PyObject* obj) {
    Py_ssize_t n = PyObject_Length(obj);
    if (n < 0) {
        PyErr_SetString(PyExc_TypeError,
                        "OHLC columns must be sized sequences or buffers");
        return -1;
    }
    return n;
}

/* NaN/inf must never reach the renderers: cc_pixel() feeds them to lround(),
 * whose behavior is undefined for non-finite input. */
static int check_finite(const double* v, Py_ssize_t n) {
    Py_ssize_t i;
    for (i = 0; i < n; i++) {
        if (!isfinite(v[i])) {
            PyErr_SetString(PyExc_ValueError,
                            "OHLC values must be finite (no NaN or inf)");
            return -1;
        }
    }
    return 0;
}

/* True when `obj` hands out exactly `n` native doubles as one flat block.
 * PyBUF_STRIDES is deliberately not requested, so a non-contiguous buffer is
 * rejected by the protocol itself rather than silently misread. */
static int try_copy_doubles(PyObject* obj, double* out, Py_ssize_t n) {
    Py_buffer view;
    int copied = 0;

    if (!PyObject_CheckBuffer(obj)) return 0;
    if (PyObject_GetBuffer(obj, &view, PyBUF_ND | PyBUF_FORMAT) != 0) {
        PyErr_Clear();
        return 0;
    }
    if (view.ndim == 1 && view.buf != NULL && view.shape != NULL &&
        view.shape[0] == n && view.itemsize == (Py_ssize_t)sizeof(double) &&
        view.format != NULL && view.format[0] == 'd' && view.format[1] == '\0') {
        memcpy(out, view.buf, (size_t)n * sizeof(double));
        copied = 1;
    }
    PyBuffer_Release(&view);
    return copied;
}

/* Same idea for epoch-second columns: 8-byte native ints ('q' or 'l'). */
static int try_copy_longs(PyObject* obj, long long* out, Py_ssize_t n) {
    Py_buffer view;
    int copied = 0;

    if (!PyObject_CheckBuffer(obj)) return 0;
    if (PyObject_GetBuffer(obj, &view, PyBUF_ND | PyBUF_FORMAT) != 0) {
        PyErr_Clear();
        return 0;
    }
    if (view.ndim == 1 && view.buf != NULL && view.shape != NULL &&
        view.shape[0] == n && view.itemsize == (Py_ssize_t)sizeof(long long) &&
        view.format != NULL && view.format[1] == '\0' &&
        (view.format[0] == 'q' || view.format[0] == 'l')) {
        memcpy(out, view.buf, (size_t)n * sizeof(long long));
        copied = 1;
    }
    PyBuffer_Release(&view);
    return copied;
}

/* Fills `out` with n doubles from a buffer or any sequence. 0 = ok. */
static int fill_doubles(PyObject* obj, double* out, Py_ssize_t n) {
    PyObject* fast;
    Py_ssize_t i;

    if (!try_copy_doubles(obj, out, n)) {
        fast = PySequence_Fast(obj, "OHLC columns must be sequences or buffers");
        if (fast == NULL) return -1;
        if (PySequence_Fast_GET_SIZE(fast) != n) {
            Py_DECREF(fast);
            PyErr_SetString(PyExc_ValueError, "OHLC columns changed size");
            return -1;
        }
        for (i = 0; i < n; i++) {
            double v = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(fast, i));
            if (v == -1.0 && PyErr_Occurred()) {
                Py_DECREF(fast);
                return -1;
            }
            out[i] = v;
        }
        Py_DECREF(fast);
    }
    return check_finite(out, n);
}

/* Fills `out` with n epoch seconds from a buffer or any sequence. 0 = ok.
 * Floats are accepted and truncated, so a float64 timestamp column works. */
static int fill_longs(PyObject* obj, long long* out, Py_ssize_t n) {
    PyObject* fast;
    Py_ssize_t i;

    if (try_copy_longs(obj, out, n)) return 0;

    fast = PySequence_Fast(obj, "ts must be a sequence or a buffer");
    if (fast == NULL) return -1;
    if (PySequence_Fast_GET_SIZE(fast) != n) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "ts changed size");
        return -1;
    }
    for (i = 0; i < n; i++) {
        PyObject* item = PySequence_Fast_GET_ITEM(fast, i);
        if (PyFloat_Check(item)) {
            out[i] = (long long)PyFloat_AS_DOUBLE(item);
        } else {
            PyObject* index = PyNumber_Index(item);
            long long v;
            if (index == NULL) {
                Py_DECREF(fast);
                return -1;
            }
            v = PyLong_AsLongLong(index);
            Py_DECREF(index);
            if (v == -1 && PyErr_Occurred()) {
                Py_DECREF(fast);
                return -1;
            }
            out[i] = v;
        }
    }
    Py_DECREF(fast);
    return 0;
}

/* Builds the OHLC capsule from four price columns and optional timestamps. */
static PyObject* py_parse_arrays(PyObject* self, PyObject* args) {
    PyObject* cols[4];
    PyObject* o_ts = Py_None;
    PyObject* capsule;
    double* vals = NULL;
    long long* ts = NULL;
    cc_ohlc_t* ohlc = NULL;
    py_ohlc_data* od = NULL;
    Py_ssize_t n, i;
    int c;

    if (!PyArg_ParseTuple(args, "OOOO|O", &cols[0], &cols[1], &cols[2],
                          &cols[3], &o_ts)) {
        return NULL;
    }

    n = arg_len(cols[0]);
    if (n < 0) return NULL;
    for (c = 1; c < 4; c++) {
        Py_ssize_t m = arg_len(cols[c]);
        if (m < 0) return NULL;
        if (m != n) {
            PyErr_SetString(PyExc_ValueError,
                            "open, high, low and close must have the same length");
            return NULL;
        }
    }
    if (o_ts != Py_None) {
        Py_ssize_t m = arg_len(o_ts);
        if (m < 0) return NULL;
        if (m != n) {
            PyErr_SetString(PyExc_ValueError,
                            "ts must have the same length as the OHLC columns");
            return NULL;
        }
    }
    if (n <= 0) {
        PyErr_SetString(PyExc_ValueError, "need at least one candle");
        return NULL;
    }
    /* The C library carries the candle count as an int. */
    if (n > (Py_ssize_t)(INT_MAX / (int)sizeof(cc_ohlc_t))) {
        PyErr_SetString(PyExc_ValueError, "too many candles");
        return NULL;
    }

    vals = (double*)malloc((size_t)n * 4 * sizeof(double));
    ohlc = (cc_ohlc_t*)calloc((size_t)n, sizeof(cc_ohlc_t));
    od = (py_ohlc_data*)malloc(sizeof(py_ohlc_data));
    if (o_ts != Py_None) {
        ts = (long long*)malloc((size_t)n * sizeof(long long));
    }
    if (vals == NULL || ohlc == NULL || od == NULL ||
        (o_ts != Py_None && ts == NULL)) {
        PyErr_NoMemory();
        goto error;
    }

    for (c = 0; c < 4; c++) {
        if (fill_doubles(cols[c], vals + (Py_ssize_t)c * n, n) != 0) goto error;
    }
    if (ts != NULL && fill_longs(o_ts, ts, n) != 0) goto error;

    for (i = 0; i < n; i++) {
        ohlc[i].open  = vals[i];
        ohlc[i].high  = vals[n + i];
        ohlc[i].low   = vals[2 * n + i];
        ohlc[i].close = vals[3 * n + i];
        ohlc[i].timestamp = (ts != NULL) ? ts[i] : 0;
    }

    od->data = ohlc;
    od->size = (int)n;
    capsule = PyCapsule_New(od, "ccharts.ohlc", &dealloc_ohlc);
    if (capsule == NULL) goto error;

    free(vals);
    free(ts);
    return capsule;

error:
    free(vals);
    free(ts);
    free(ohlc);
    free(od);
    return NULL;
}

/* Shared argument parsing + capsule handling for both renderers.
 * Returns the py_ohlc_data on success, NULL with an exception set on error. */
static py_ohlc_data* prepare_render(PyObject* args, int* width, int* height,
                                    cc_settings_t* settings) {
    PyObject* capsule;
    const char *rise_color = NULL, *fall_color = NULL, *bg_color = NULL, *area_color = NULL;
    int single_color = 0, show_prices = 0, show_times = 0;

    /* Arg format "Oii|zzzziii": capsule, width, height, then optional
     * rise/fall/bg/area colors (str or None) and the int settings flags. */
    if (!PyArg_ParseTuple(args, "Oii|zzzziii", &capsule, width, height,
                          &rise_color, &fall_color, &bg_color, &area_color,
                          &single_color, &show_prices, &show_times)) {
        return NULL;
    }

    if (!check_dimensions(*width, *height)) {
        PyErr_SetString(PyExc_ValueError,
                        "width and height must be positive integers within "
                        "CC_MAX_DIM and CC_MAX_CELLS limits");
        return NULL;
    }

    if (!PyCapsule_IsValid(capsule, "ccharts.ohlc")) {
        PyErr_SetString(PyExc_ValueError, "invalid data capsule");
        return NULL;
    }
    py_ohlc_data* od = (py_ohlc_data*)PyCapsule_GetPointer(capsule, "ccharts.ohlc");
    if (od == NULL || od->data == NULL || od->size <= 0) {
        PyErr_SetString(PyExc_ValueError, "capsule holds no chart data");
        return NULL;
    }

    settings->rise_color = rise_color;
    settings->fall_color = fall_color;
    settings->bg_color = bg_color;
    settings->area_color = area_color;
    settings->single_color = single_color;
    settings->show_prices = show_prices;
    settings->show_times = show_times;
    return od;
}

/* Renders a line chart from the capsule and returns it as a str. */
static PyObject* py_create_line(PyObject* self, PyObject* args) {
    int width, height;
    cc_settings_t settings = {0};
    py_ohlc_data* od = prepare_render(args, &width, &height, &settings);
    if (od == NULL) {
        return NULL;
    }

    char* chart = cc_line_create(od->data, od->size, width, height, &settings);
    if (!chart) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    PyObject* result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* Same as py_create_line, but renders a candle chart. */
static PyObject* py_create_candle(PyObject* self, PyObject* args) {
    int width, height;
    cc_settings_t settings = {0};
    py_ohlc_data* od = prepare_render(args, &width, &height, &settings);
    if (od == NULL) {
        return NULL;
    }

    char* chart = cc_candle_create(od->data, od->size, width, height, &settings);
    if (!chart) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    PyObject* result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* ================================ Pie ================================
 * A pie is a set of (label, value) slices, not OHLC columns, so this path
 * does not touch the capsule. Labels and values are read from parallel
 * sequences (str-or-None and number respectively), NaN/inf is rejected with
 * the same finite guard as the OHLC paths, and the rows are handed to
 * cc_pie_create, which owns every piece of the render.
 * ===================================================================== */

/* Fills `slices` (caller-freed) from parallel label/value sequences. The
 * label pointers are borrowed from the Python str objects and only need to
 * live for the synchronous cc_pie_create call. Returns NULL with an
 * exception set on any mismatch, non-numeric value, or non-finite value. */
static cc_pie_slice_t* build_pie_slices(PyObject* o_labels, PyObject* o_values,
                                        Py_ssize_t* out_count) {
    PyObject* fast_labels;
    PyObject* fast_values;
    Py_ssize_t n, i;
    cc_pie_slice_t* slices;

    fast_labels = PySequence_Fast(o_labels, "labels must be a sequence");
    if (fast_labels == NULL) return NULL;
    fast_values = PySequence_Fast(o_values, "values must be a sequence");
    if (fast_values == NULL) {
        Py_DECREF(fast_labels);
        return NULL;
    }
    n = PySequence_Fast_GET_SIZE(fast_labels);
    if (PySequence_Fast_GET_SIZE(fast_values) != n) {
        Py_DECREF(fast_labels);
        Py_DECREF(fast_values);
        PyErr_SetString(PyExc_ValueError,
                        "labels and values must have the same length");
        return NULL;
    }
    if (n <= 0) {
        Py_DECREF(fast_labels);
        Py_DECREF(fast_values);
        PyErr_SetString(PyExc_ValueError, "need at least one slice");
        return NULL;
    }

    slices = (cc_pie_slice_t*)calloc((size_t)n, sizeof(cc_pie_slice_t));
    if (slices == NULL) {
        Py_DECREF(fast_labels);
        Py_DECREF(fast_values);
        PyErr_NoMemory();
        return NULL;
    }

    for (i = 0; i < n; i++) {
        PyObject* label = PySequence_Fast_GET_ITEM(fast_labels, i);
        PyObject* value = PySequence_Fast_GET_ITEM(fast_values, i);
        double v;

        if (label == Py_None) {
            slices[i].label = NULL;
        } else if (PyUnicode_Check(label)) {
            slices[i].label = PyUnicode_AsUTF8(label);
            if (slices[i].label == NULL) {
                free(slices);
                Py_DECREF(fast_labels);
                Py_DECREF(fast_values);
                return NULL;
            }
        } else {
            free(slices);
            Py_DECREF(fast_labels);
            Py_DECREF(fast_values);
            PyErr_SetString(PyExc_TypeError, "labels must be strings or None");
            return NULL;
        }

        v = PyFloat_AsDouble(value);
        if (v == -1.0 && PyErr_Occurred()) {
            free(slices);
            Py_DECREF(fast_labels);
            Py_DECREF(fast_values);
            return NULL;
        }
        if (!isfinite(v)) {
            free(slices);
            Py_DECREF(fast_labels);
            Py_DECREF(fast_values);
            PyErr_SetString(PyExc_ValueError,
                            "slice values must be finite (no NaN or inf)");
            return NULL;
        }
        slices[i].value = v;
    }

    Py_DECREF(fast_labels);
    Py_DECREF(fast_values);
    *out_count = n;
    return slices;
}

/* Builds a NULL-terminated array of ANSI escape strings (what
 * cc_pie_settings_t expects) from a sequence of str, or leaves *out NULL for
 * the default palette when o_colors is None. Returns 0 or -1 with an
 * exception set. */
static int build_pie_colors(PyObject* o_colors, const char*** out) {
    PyObject* fast;
    Py_ssize_t n, i;
    const char** arr;

    *out = NULL;
    if (o_colors == Py_None) return 0;

    fast = PySequence_Fast(o_colors,
                           "colors must be a sequence of ANSI escape strings");
    if (fast == NULL) return -1;
    n = PySequence_Fast_GET_SIZE(fast);
    arr = (const char**)calloc((size_t)n + 1, sizeof(const char*));
    if (arr == NULL) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return -1;
    }
    for (i = 0; i < n; i++) {
        PyObject* item = PySequence_Fast_GET_ITEM(fast, i);
        if (!PyUnicode_Check(item)) {
            free(arr);
            Py_DECREF(fast);
            PyErr_SetString(PyExc_TypeError,
                            "colors must be strings or None");
            return -1;
        }
        arr[i] = PyUnicode_AsUTF8(item);
        if (arr[i] == NULL) {
            free(arr);
            Py_DECREF(fast);
            return -1;
        }
    }
    Py_DECREF(fast);
    *out = arr;
    return 0;
}

/* Renders a pie/donut from parallel label/value sequences. */
static PyObject* py_create_pie(PyObject* self, PyObject* args) {
    PyObject* o_labels;
    PyObject* o_values;
    PyObject* o_colors = Py_None;
    const char* bg_color = NULL;
    int width, height;
    int donut = 0, show_legend = 1, show_pct = 0;
    double slice_gap = 0.0;
    double inner_ratio = -1.0;      /* sentinel: donut flag decides */
    int legend_format = CC_PIE_LEGEND_VALUE;
    double start_angle = -1.0;      /* sentinel: CC_PI/2 */
    int counter_clockwise = 0;
    const char* center_text = NULL;
    cc_pie_slice_t* slices;
    const char** colors;
    cc_pie_settings_t settings;
    Py_ssize_t count;
    char* chart;
    PyObject* result;

    if (!PyArg_ParseTuple(args, "OOii|iOziiddidiz", &o_labels, &o_values,
                          &width, &height, &donut, &o_colors, &bg_color,
                          &show_legend, &show_pct, &slice_gap, &inner_ratio,
                          &legend_format, &start_angle, &counter_clockwise,
                          &center_text)) {
        return NULL;
    }
    if (!check_dimensions(width, height)) {
        PyErr_SetString(PyExc_ValueError,
                        "width and height must be positive integers within "
                        "CC_MAX_DIM and CC_MAX_CELLS limits");
        return NULL;
    }
    /* Non-finite option doubles must never reach the renderer's geometry. */
    if (!isfinite(slice_gap) || !isfinite(inner_ratio) ||
        !isfinite(start_angle)) {
        PyErr_SetString(PyExc_ValueError,
                        "slice_gap, inner_radius_ratio and start_angle must "
                        "be finite (no NaN or inf)");
        return NULL;
    }

    slices = build_pie_slices(o_labels, o_values, &count);
    if (slices == NULL) return NULL;
    if (build_pie_colors(o_colors, &colors) != 0) {
        free(slices);
        return NULL;
    }

    memset(&settings, 0, sizeof(settings));
    settings.bg_color = bg_color;
    settings.colors = colors;
    settings.donut = donut;
    settings.show_legend = show_legend;
    settings.show_pct = show_pct;
    settings.slice_gap = slice_gap;
    settings.inner_radius_ratio = inner_ratio;
    settings.legend_format = legend_format;
    settings.start_angle = start_angle;
    settings.counter_clockwise = counter_clockwise;
    settings.center_text = center_text;

    chart = cc_pie_create(slices, (int)count, width, height, &settings);
    free(slices);
    free(colors);
    if (chart == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* ============================= Histogram =============================
 * A histogram renders a 1-D sequence of scalar samples (no OHLC capsule).
 * The samples are read through the buffer protocol / sequence fast path
 * exactly like fill_doubles, NaN/inf is rejected with a sample-appropriate
 * message, and the rows are handed to cc_hist_create. Empty samples render
 * the empty string, matching the C renderer. min_value/max_value use NaN as
 * the "auto" sentinel (the Python layer passes NaN for None). */

/* Fills `out` with n doubles from a buffer or any sequence, rejecting any
 * NaN/inf sample before it can reach the renderer's bin math. 0 = ok. */
static int fill_hist_samples(PyObject* obj, double* out, Py_ssize_t n) {
    PyObject* fast;
    Py_ssize_t i;

    if (!try_copy_doubles(obj, out, n)) {
        fast = PySequence_Fast(obj, "samples must be a sequence or a buffer");
        if (fast == NULL) return -1;
        if (PySequence_Fast_GET_SIZE(fast) != n) {
            Py_DECREF(fast);
            PyErr_SetString(PyExc_ValueError, "samples changed size");
            return -1;
        }
        for (i = 0; i < n; i++) {
            double v = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(fast, i));
            if (v == -1.0 && PyErr_Occurred()) {
                Py_DECREF(fast);
                return -1;
            }
            out[i] = v;
        }
        Py_DECREF(fast);
    }
    for (i = 0; i < n; i++) {
        if (!isfinite(out[i])) {
            PyErr_SetString(PyExc_ValueError,
                            "histogram samples must be finite (no NaN or inf)");
            return -1;
        }
    }
    return 0;
}

/* Renders a histogram of a scalar sample sequence. */
static PyObject* py_create_hist(PyObject* self, PyObject* args) {
    PyObject* o_samples;
    const char* rise_color = NULL;
    const char* bg_color = NULL;
    int width, height;
    int bin_count = 0, show_bins = 0, show_prices = 0;
    double min_value = 0.0 / 0.0;   /* NaN = auto sentinel */
    double max_value = 0.0 / 0.0;   /* NaN = auto sentinel */
    Py_ssize_t n;
    double* vals = NULL;
    cc_hist_settings_t settings;
    char* chart;
    PyObject* result;

    /* "Oii|zziddii": samples, width, height, then optional rise/bg colors,
     * bin_count, min/max (doubles), show_bins, show_prices. */
    if (!PyArg_ParseTuple(args, "Oii|zziddii", &o_samples, &width, &height,
                          &rise_color, &bg_color, &bin_count, &min_value,
                          &max_value, &show_bins, &show_prices)) {
        return NULL;
    }
    if (!check_dimensions(width, height)) {
        PyErr_SetString(PyExc_ValueError,
                        "width and height must be positive integers within "
                        "CC_MAX_DIM and CC_MAX_CELLS limits");
        return NULL;
    }

    n = arg_len(o_samples);
    if (n < 0) return NULL;
    if (n > (Py_ssize_t)(INT_MAX / (int)sizeof(double))) {
        PyErr_SetString(PyExc_ValueError, "too many samples");
        return NULL;
    }

    vals = (double*)malloc((size_t)((n > 0) ? n : 1) * sizeof(double));
    if (vals == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    if (n > 0 && fill_hist_samples(o_samples, vals, n) != 0) {
        free(vals);
        return NULL;
    }

    memset(&settings, 0, sizeof(settings));
    settings.rise_color = rise_color;
    settings.bg_color = bg_color;
    settings.bin_count = bin_count;
    settings.min_value = min_value;
    settings.max_value = max_value;
    settings.show_bins = show_bins;
    settings.show_prices = show_prices;

    chart = cc_hist_create(vals, (int)n, width, height, &settings);
    free(vals);
    if (chart == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* ============================= Sparkline =============================
 * A sparkline renders a 1-D sequence of scalar samples into a tiny axis-less
 * trend line (no OHLC capsule). The samples are read through the same buffer
 * protocol / sequence fast path as the histogram, NaN/inf is rejected, and
 * the rows are handed to cc_spark_create. Empty samples render the empty
 * string, matching the C renderer. */

/* Fills `out` with n doubles from a buffer or any sequence, rejecting any
 * NaN/inf sample before it can reach the renderer's column math. 0 = ok. */
static int fill_spark_samples(PyObject* obj, double* out, Py_ssize_t n) {
    PyObject* fast;
    Py_ssize_t i;

    if (!try_copy_doubles(obj, out, n)) {
        fast = PySequence_Fast(obj, "samples must be a sequence or a buffer");
        if (fast == NULL) return -1;
        if (PySequence_Fast_GET_SIZE(fast) != n) {
            Py_DECREF(fast);
            PyErr_SetString(PyExc_ValueError, "samples changed size");
            return -1;
        }
        for (i = 0; i < n; i++) {
            double v = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(fast, i));
            if (v == -1.0 && PyErr_Occurred()) {
                Py_DECREF(fast);
                return -1;
            }
            out[i] = v;
        }
        Py_DECREF(fast);
    }
    for (i = 0; i < n; i++) {
        if (!isfinite(out[i])) {
            PyErr_SetString(PyExc_ValueError,
                            "sparkline samples must be finite (no NaN or inf)");
            return -1;
        }
    }
    return 0;
}

/* Renders a sparkline of a scalar sample sequence. */
static PyObject* py_create_spark(PyObject* self, PyObject* args) {
    PyObject* o_samples;
    const char* rise_color = NULL;
    const char* area_color = NULL;
    int width, height;
    int min_above = 0, min_below = 0;
    Py_ssize_t n;
    double* vals = NULL;
    cc_spark_settings_t settings;
    char* chart;
    PyObject* result;

    /* "Oii|zzii": samples, width, height, then optional rise/area colors
     * and min_above/min_below sub-pixel margins. */
    if (!PyArg_ParseTuple(args, "Oii|zzii", &o_samples, &width, &height,
                          &rise_color, &area_color, &min_above, &min_below)) {
        return NULL;
    }
    if (!check_dimensions(width, height)) {
        PyErr_SetString(PyExc_ValueError,
                        "width and height must be positive integers within "
                        "CC_MAX_DIM and CC_MAX_CELLS limits");
        return NULL;
    }

    n = arg_len(o_samples);
    if (n < 0) return NULL;
    if (n > (Py_ssize_t)(INT_MAX / (int)sizeof(double))) {
        PyErr_SetString(PyExc_ValueError, "too many samples");
        return NULL;
    }

    vals = (double*)malloc((size_t)((n > 0) ? n : 1) * sizeof(double));
    if (vals == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    if (n > 0 && fill_spark_samples(o_samples, vals, n) != 0) {
        free(vals);
        return NULL;
    }

    memset(&settings, 0, sizeof(settings));
    settings.rise_color = rise_color;
    settings.area_color = area_color;
    settings.min_above = min_above;
    settings.min_below = min_below;

    chart = cc_spark_create(vals, (int)n, width, height, &settings);
    free(vals);
    if (chart == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* =============================== Bar ================================
 * A bar chart renders an ordered list of (label, value) pairs into vertical
 * bars from a zero baseline (not OHLC columns, so no capsule). Labels and
 * values are read from parallel sequences exactly like the pie, NaN/inf is
 * rejected with a bar-appropriate message (negative values are clamped to
 * zero by the renderer), and the rows are handed to cc_bar_create, which owns
 * every piece of the render.
 * ===================================================================== */

/* Fills `items` (caller-freed) from parallel label/value sequences. The
 * label pointers are borrowed from the Python str objects and only need to
 * live for the synchronous cc_bar_create call. Returns NULL with an
 * exception set on any mismatch, non-numeric value, or non-finite value. */
static cc_bar_item_t* build_bar_items(PyObject* o_labels, PyObject* o_values,
                                      Py_ssize_t* out_count) {
    PyObject* fast_labels;
    PyObject* fast_values;
    Py_ssize_t n, i;
    cc_bar_item_t* items;

    fast_labels = PySequence_Fast(o_labels, "labels must be a sequence");
    if (fast_labels == NULL) return NULL;
    fast_values = PySequence_Fast(o_values, "values must be a sequence");
    if (fast_values == NULL) {
        Py_DECREF(fast_labels);
        return NULL;
    }
    n = PySequence_Fast_GET_SIZE(fast_labels);
    if (PySequence_Fast_GET_SIZE(fast_values) != n) {
        Py_DECREF(fast_labels);
        Py_DECREF(fast_values);
        PyErr_SetString(PyExc_ValueError,
                        "labels and values must have the same length");
        return NULL;
    }
    if (n <= 0) {
        Py_DECREF(fast_labels);
        Py_DECREF(fast_values);
        PyErr_SetString(PyExc_ValueError, "need at least one bar");
        return NULL;
    }

    items = (cc_bar_item_t*)calloc((size_t)n, sizeof(cc_bar_item_t));
    if (items == NULL) {
        Py_DECREF(fast_labels);
        Py_DECREF(fast_values);
        PyErr_NoMemory();
        return NULL;
    }

    for (i = 0; i < n; i++) {
        PyObject* label = PySequence_Fast_GET_ITEM(fast_labels, i);
        PyObject* value = PySequence_Fast_GET_ITEM(fast_values, i);
        double v;

        if (label == Py_None) {
            items[i].label = NULL;
        } else if (PyUnicode_Check(label)) {
            items[i].label = PyUnicode_AsUTF8(label);
            if (items[i].label == NULL) {
                free(items);
                Py_DECREF(fast_labels);
                Py_DECREF(fast_values);
                return NULL;
            }
        } else {
            free(items);
            Py_DECREF(fast_labels);
            Py_DECREF(fast_values);
            PyErr_SetString(PyExc_TypeError, "labels must be strings or None");
            return NULL;
        }

        v = PyFloat_AsDouble(value);
        if (v == -1.0 && PyErr_Occurred()) {
            free(items);
            Py_DECREF(fast_labels);
            Py_DECREF(fast_values);
            return NULL;
        }
        if (!isfinite(v)) {
            free(items);
            Py_DECREF(fast_labels);
            Py_DECREF(fast_values);
            PyErr_SetString(PyExc_ValueError,
                            "bar values must be finite (no NaN or inf)");
            return NULL;
        }
        items[i].value = v;
    }

    Py_DECREF(fast_labels);
    Py_DECREF(fast_values);
    *out_count = n;
    return items;
}

/* Renders a bar chart from parallel label/value sequences. */
static PyObject* py_create_bar(PyObject* self, PyObject* args) {
    PyObject* o_labels;
    PyObject* o_values;
    const char* rise_color = NULL;
    const char* bg_color = NULL;
    int width, height;
    int show_labels = 0, show_prices = 0;
    cc_bar_item_t* items;
    cc_bar_settings_t settings;
    Py_ssize_t count;
    char* chart;
    PyObject* result;

    /* "OOii|zzii": labels, values, width, height, then optional rise/bg
     * colors and the show_labels/show_prices flags. */
    if (!PyArg_ParseTuple(args, "OOii|zzii", &o_labels, &o_values,
                          &width, &height, &rise_color, &bg_color,
                          &show_labels, &show_prices)) {
        return NULL;
    }
    if (!check_dimensions(width, height)) {
        PyErr_SetString(PyExc_ValueError,
                        "width and height must be positive integers within "
                        "CC_MAX_DIM and CC_MAX_CELLS limits");
        return NULL;
    }

    items = build_bar_items(o_labels, o_values, &count);
    if (items == NULL) return NULL;

    memset(&settings, 0, sizeof(settings));
    settings.rise_color = rise_color;
    settings.bg_color = bg_color;
    settings.show_labels = show_labels;
    settings.show_prices = show_prices;

    chart = cc_bar_create(items, (int)count, width, height, &settings);
    free(items);
    if (chart == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* Converts a Python sequence of str/None into a NULL-terminated array of
 * UTF-8 const char* (the pointers borrow from the Python strings and stay
 * valid while the GIL is held). Returns NULL with an exception set on error;
 * the caller frees the returned array (not the borrowed strings). */
static char** py_seq_to_cstrings(PyObject* o, Py_ssize_t* out_n) {
    PyObject* fast;
    Py_ssize_t n, i;
    char** out;

    fast = PySequence_Fast(o, "expected a sequence of strings or None");
    if (fast == NULL) return NULL;
    n = PySequence_Fast_GET_SIZE(fast);
    out = (char**)calloc((size_t)n + 1, sizeof(char*));   /* +1 for NULL terminator */
    if (out == NULL) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for (i = 0; i < n; i++) {
        PyObject* item = PySequence_Fast_GET_ITEM(fast, i);
        if (item == Py_None) {
            out[i] = NULL;
            continue;
        }
        if (!PyUnicode_Check(item)) {
            free(out);
            Py_DECREF(fast);
            PyErr_SetString(PyExc_TypeError, "expected strings or None");
            return NULL;
        }
        out[i] = (char*)PyUnicode_AsUTF8(item);
        if (out[i] == NULL) {
            free(out);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);
    *out_n = n;
    return out;
}

/* Builds the cc_stack_series_t array (and its flat values buffer) from
 * parallel `series_names` (list of str/None) and `series_values` (list of
 * lists, one values sequence per series that must each be the same length =
 * the category count). Non-finite values are rejected. Returns the series
 * array (caller frees BOTH it and the *out_flat buffer), or NULL with an
 * exception set. The `name` pointers borrow from the Python strings and stay
 * valid while the GIL is held. */
static cc_stack_series_t* build_stack_series(PyObject* o_names, PyObject* o_matrix,
                                             double** out_flat, int* out_cats,
                                             Py_ssize_t* out_count) {
    PyObject* fast_names;
    PyObject* fast_matrix;
    Py_ssize_t n, cats, i;
    cc_stack_series_t* series;
    double* flat;

    fast_names = PySequence_Fast(o_names, "series names must be a sequence");
    if (fast_names == NULL) return NULL;
    fast_matrix = PySequence_Fast(o_matrix, "series values must be a sequence");
    if (fast_matrix == NULL) {
        Py_DECREF(fast_names);
        return NULL;
    }
    n = PySequence_Fast_GET_SIZE(fast_names);
    if (PySequence_Fast_GET_SIZE(fast_matrix) != n) {
        Py_DECREF(fast_names);
        Py_DECREF(fast_matrix);
        PyErr_SetString(PyExc_ValueError,
                        "series names and values must have the same length");
        return NULL;
    }
    if (n <= 0) {
        Py_DECREF(fast_names);
        Py_DECREF(fast_matrix);
        PyErr_SetString(PyExc_ValueError, "need at least one series");
        return NULL;
    }

    /* The category count is the first series' length; all must match. */
    {
        PyObject* first = PySequence_Fast_GET_ITEM(fast_matrix, 0);
        PyObject* fast_first = PySequence_Fast(first,
                                               "each series values must be a sequence");
        if (fast_first == NULL) {
            Py_DECREF(fast_names);
            Py_DECREF(fast_matrix);
            return NULL;
        }
        cats = PySequence_Fast_GET_SIZE(fast_first);
        Py_DECREF(fast_first);
    }
    if (cats <= 0) {
        Py_DECREF(fast_names);
        Py_DECREF(fast_matrix);
        PyErr_SetString(PyExc_ValueError, "need at least one value per series");
        return NULL;
    }

    series = (cc_stack_series_t*)calloc((size_t)n, sizeof(cc_stack_series_t));
    flat = (double*)calloc((size_t)n * (size_t)cats, sizeof(double));
    if (series == NULL || flat == NULL) {
        free(series);
        free(flat);
        Py_DECREF(fast_names);
        Py_DECREF(fast_matrix);
        PyErr_NoMemory();
        return NULL;
    }

    for (i = 0; i < n; i++) {
        PyObject* name = PySequence_Fast_GET_ITEM(fast_names, i);
        PyObject* vals = PySequence_Fast_GET_ITEM(fast_matrix, i);
        PyObject* fast_vals;
        Py_ssize_t vlen, j;

        series[i].values = flat + (size_t)i * (size_t)cats;

        if (name == Py_None) {
            series[i].name = NULL;
        } else if (PyUnicode_Check(name)) {
            series[i].name = PyUnicode_AsUTF8(name);
            if (series[i].name == NULL) goto error;
        } else {
            PyErr_SetString(PyExc_TypeError, "series names must be strings or None");
            goto error;
        }

        fast_vals = PySequence_Fast(vals, "each series values must be a sequence");
        if (fast_vals == NULL) goto error;
        vlen = PySequence_Fast_GET_SIZE(fast_vals);
        if (vlen != cats) {
            Py_DECREF(fast_vals);
            PyErr_SetString(PyExc_ValueError,
                            "all series must have the same number of values");
            goto error;
        }
        for (j = 0; j < vlen; j++) {
            PyObject* o = PySequence_Fast_GET_ITEM(fast_vals, j);
            double v = PyFloat_AsDouble(o);
            if (v == -1.0 && PyErr_Occurred()) {
                Py_DECREF(fast_vals);
                goto error;
            }
            if (!isfinite(v)) {
                Py_DECREF(fast_vals);
                PyErr_SetString(PyExc_ValueError,
                                "stacked bar values must be finite (no NaN or inf)");
                goto error;
            }
            flat[(size_t)i * (size_t)cats + j] = v;
        }
        Py_DECREF(fast_vals);
    }

    Py_DECREF(fast_names);
    Py_DECREF(fast_matrix);
    *out_flat = flat;
    *out_cats = (int)cats;
    *out_count = n;
    return series;

error:
    free(series);
    free(flat);
    Py_DECREF(fast_names);
    Py_DECREF(fast_matrix);
    return NULL;
}

/* Renders a stacked bar chart from parallel series names and a values
 * matrix. */
static PyObject* py_create_stack(PyObject* self, PyObject* args) {
    PyObject* o_names;
    PyObject* o_matrix;
    const char* bg_color = NULL;
    PyObject* o_colors = NULL;
    PyObject* o_cat_labels = NULL;
    int width, height;
    int show_labels = 0, show_prices = 0;
    cc_stack_series_t* series;
    double* flat;
    char** colors = NULL;
    char** cat_labels = NULL;
    int cats;
    Py_ssize_t count;
    cc_stack_settings_t settings;
    char* chart;
    PyObject* result;

    /* "OOii|zOOii": names, matrix, width, height, then optional bg color,
     * colors list, category labels list, and the show_labels/show_prices
     * flags. */
    if (!PyArg_ParseTuple(args, "OOii|zOOii", &o_names, &o_matrix,
                          &width, &height, &bg_color, &o_colors,
                          &o_cat_labels, &show_labels, &show_prices)) {
        return NULL;
    }
    if (!check_dimensions(width, height)) {
        PyErr_SetString(PyExc_ValueError,
                        "width and height must be positive integers within "
                        "CC_MAX_DIM and CC_MAX_CELLS limits");
        return NULL;
    }

    series = build_stack_series(o_names, o_matrix, &flat, &cats, &count);
    if (series == NULL) return NULL;

    if (o_colors != NULL && o_colors != Py_None) {
        Py_ssize_t nc;
        colors = py_seq_to_cstrings(o_colors, &nc);
        if (colors == NULL) {
            free(series);
            free(flat);
            return NULL;
        }
    }
    if (o_cat_labels != NULL && o_cat_labels != Py_None) {
        Py_ssize_t ncl;
        cat_labels = py_seq_to_cstrings(o_cat_labels, &ncl);
        if (cat_labels == NULL) {
            free(series);
            free(flat);
            free(colors);
            return NULL;
        }
    }

    memset(&settings, 0, sizeof(settings));
    settings.bg_color = bg_color;
    settings.colors = (const char* const*)colors;
    settings.cat_labels = (const char* const*)cat_labels;
    settings.series = (int)count;
    settings.cats = cats;
    settings.show_labels = show_labels;
    settings.show_prices = show_prices;

    chart = cc_stack_create(series, (int)count, width, height, &settings);
    free(series);
    free(flat);
    free(colors);
    free(cat_labels);
    if (chart == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* ============================== Heatmap ==============================
 * A heatmap renders a 2-D matrix of scalar values (list of lists) as a grid
 * of colored cells. The matrix is flattened row-major into one double array
 * and handed to cc_heat_create; NaN/inf is rejected like every other path.
 * Labels are optional parallel lists (row_labels of `rows` entries,
 * col_labels of `cols` entries) or None.
 * ========================================================================= */

/* Flattens a 2-D `matrix` (a sequence of `rows` sequences, each `cols` long)
 * into *out_flat (caller frees) and returns rows/cols through the outputs,
 * rejecting ragged rows, empty matrices and any non-finite entry. 0 = ok. */
static int build_heat_matrix(PyObject* o_matrix, double** out_flat,
                             Py_ssize_t* out_rows, Py_ssize_t* out_cols) {
    PyObject* fast;
    Py_ssize_t rows, cols, r, c;

    fast = PySequence_Fast(o_matrix, "heatmap values must be a 2-D sequence");
    if (fast == NULL) return -1;
    rows = PySequence_Fast_GET_SIZE(fast);
    if (rows <= 0) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "heatmap needs at least one row");
        return -1;
    }

    {
        PyObject* first = PySequence_Fast_GET_ITEM(fast, 0);
        PyObject* ffast = PySequence_Fast(first, "each row must be a sequence");
        if (ffast == NULL) { Py_DECREF(fast); return -1; }
        cols = PySequence_Fast_GET_SIZE(ffast);
        Py_DECREF(ffast);
    }
    if (cols <= 0) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "heatmap needs at least one column");
        return -1;
    }
    if (rows * cols > (Py_ssize_t)(INT_MAX / (int)sizeof(double))) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "too many heatmap values");
        return -1;
    }

    {
        double* flat = (double*)calloc((size_t)rows * (size_t)cols, sizeof(double));
        if (flat == NULL) {
            Py_DECREF(fast);
            PyErr_NoMemory();
            return -1;
        }
        for (r = 0; r < rows; r++) {
            PyObject* row = PySequence_Fast_GET_ITEM(fast, r);
            PyObject* rfast = PySequence_Fast(row, "each row must be a sequence");
            if (rfast == NULL) { free(flat); Py_DECREF(fast); return -1; }
            if (PySequence_Fast_GET_SIZE(rfast) != cols) {
                Py_DECREF(rfast);
                Py_DECREF(fast);
                free(flat);
                PyErr_SetString(PyExc_ValueError,
                                "all heatmap rows must have the same length");
                return -1;
            }
            for (c = 0; c < cols; c++) {
                PyObject* o = PySequence_Fast_GET_ITEM(rfast, c);
                double v = PyFloat_AsDouble(o);
                if (v == -1.0 && PyErr_Occurred()) {
                    Py_DECREF(rfast);
                    Py_DECREF(fast);
                    free(flat);
                    return -1;
                }
                if (!isfinite(v)) {
                    Py_DECREF(rfast);
                    Py_DECREF(fast);
                    free(flat);
                    PyErr_SetString(PyExc_ValueError,
                                    "heatmap values must be finite (no NaN or inf)");
                    return -1;
                }
                flat[(size_t)r * (size_t)cols + (size_t)c] = v;
            }
            Py_DECREF(rfast);
        }
        Py_DECREF(fast);
        *out_flat = flat;
    }

    *out_rows = rows;
    *out_cols = cols;
    return 0;
}

static PyObject* py_create_heat(PyObject* self, PyObject* args) {
    PyObject* o_matrix;
    const char* low_color = NULL;
    const char* high_color = NULL;
    const char* mid_color = NULL;
    const char* bg_color = NULL;
    PyObject* o_row_labels = NULL;
    PyObject* o_col_labels = NULL;
    int width, height;
    int show_labels = 0;
    Py_ssize_t rows, cols;
    double* flat = NULL;
    char** row_labels = NULL;
    char** col_labels = NULL;
    cc_heat_settings_t settings;
    char* chart;
    PyObject* result;

    /* "Oii|zzzzOOi": matrix, width, height, then optional low/high/mid/bg
     * colors (str or None), row_labels list, col_labels list, show_labels. */
    if (!PyArg_ParseTuple(args, "Oii|zzzzOOi", &o_matrix, &width, &height,
                          &low_color, &high_color, &mid_color, &bg_color,
                          &o_row_labels, &o_col_labels, &show_labels)) {
        return NULL;
    }
    if (!check_dimensions(width, height)) {
        PyErr_SetString(PyExc_ValueError,
                        "width and height must be positive integers within "
                        "CC_MAX_DIM and CC_MAX_CELLS limits");
        return NULL;
    }

    if (build_heat_matrix(o_matrix, &flat, &rows, &cols) != 0) return NULL;

    if (o_row_labels != NULL && o_row_labels != Py_None) {
        Py_ssize_t nrl;
        row_labels = py_seq_to_cstrings(o_row_labels, &nrl);
        if (row_labels == NULL) {
            free(flat);
            return NULL;
        }
    }
    if (o_col_labels != NULL && o_col_labels != Py_None) {
        Py_ssize_t ncl;
        col_labels = py_seq_to_cstrings(o_col_labels, &ncl);
        if (col_labels == NULL) {
            free(flat);
            free(row_labels);
            return NULL;
        }
    }

    memset(&settings, 0, sizeof(settings));
    settings.low_color = low_color;
    settings.high_color = high_color;
    settings.mid_color = mid_color;
    settings.bg_color = bg_color;
    settings.row_labels = (const char* const*)row_labels;
    settings.col_labels = (const char* const*)col_labels;
    settings.show_labels = show_labels;

    chart = cc_heat_create(flat, (int)rows, (int)cols, width, height, &settings);
    free(flat);
    free(row_labels);
    free(col_labels);
    if (chart == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "failed to create chart");
        return NULL;
    }

    result = PyUnicode_FromString(chart);
    free(chart);
    return result;
}

/* Module method table + module definition. */
static PyMethodDef CChartsMethods[] = {
    {"parse_json", py_parse_json, METH_VARARGS, "convert JSON data into OHLC format"},
    {"parse_arrays", py_parse_arrays, METH_VARARGS, "convert OHLC columns into OHLC format"},
    {"create_line", py_create_line, METH_VARARGS, "create a line chart"},
    {"create_candle", py_create_candle, METH_VARARGS, "create a candle chart"},
    {"create_pie", py_create_pie, METH_VARARGS, "create a pie or donut chart"},
    {"create_hist", py_create_hist, METH_VARARGS, "create a histogram chart"},
    {"create_spark", py_create_spark, METH_VARARGS, "create a sparkline chart"},
    {"create_bar", py_create_bar, METH_VARARGS, "create a bar chart"},
    {"create_stack", py_create_stack, METH_VARARGS, "create a stacked bar chart"},
    {"create_heat", py_create_heat, METH_VARARGS, "create a heatmap chart"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef cchartsmodule = {
    PyModuleDef_HEAD_INIT,
    "ccharts._core",
    NULL,
    -1,
    CChartsMethods
};

PyMODINIT_FUNC PyInit__core(void) {
    return PyModule_Create(&cchartsmodule);
}