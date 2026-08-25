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

/* Module method table + module definition. */
static PyMethodDef CChartsMethods[] = {
    {"parse_json", py_parse_json, METH_VARARGS, "convert JSON data into OHLC format"},
    {"parse_arrays", py_parse_arrays, METH_VARARGS, "convert OHLC columns into OHLC format"},
    {"create_line", py_create_line, METH_VARARGS, "create a line chart"},
    {"create_candle", py_create_candle, METH_VARARGS, "create a candle chart"},
    {"create_pie", py_create_pie, METH_VARARGS, "create a pie or donut chart"},
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