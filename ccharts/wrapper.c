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
 *   create_line(capsule, width, height, rise, fall, bg, area, single, prices, times)
 *   create_candle(...)                                     -> str
 *       Renders a chart from the capsule's data and returns the ANSI-colored
 *       string. Every settings flag maps 1:1 to cc_settings_t fields.
 *       Raises ValueError for non-positive or over-limit dimensions
 *       (CC_MAX_DIM / CC_MAX_CELLS from ccharts.h) and for invalid capsules.
 *
 * Build: `make test-py` (see setup.py).
 */

#define PY_SSIZE_T_CLEAN
#define CCHARTS_IMPLEMENTATION
#include <Python.h>
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

    py_ohlc_data* od = (py_ohlc_data*)malloc(sizeof(py_ohlc_data));
    if (od == NULL) {
        free(ohlc);
        PyErr_NoMemory();
        return NULL;
    }
    od->data = ohlc;
    od->size = size;

    return PyCapsule_New(od, "ccharts.ohlc", &dealloc_ohlc);
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

/* Module method table + module definition. */
static PyMethodDef CChartsMethods[] = {
    {"parse_json", py_parse_json, METH_VARARGS, "convert JSON data into OHLC format"},
    {"create_line", py_create_line, METH_VARARGS, "create a line chart"},
    {"create_candle", py_create_candle, METH_VARARGS, "create a candle chart"},
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