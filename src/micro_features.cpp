// src/micro_features.cpp
#define PY_SSIZE_T_CLEAN
#ifndef Py_LIMITED_API
// Target Python 3.9 stable ABI (adjust if you set a different floor)
#define Py_LIMITED_API 0x03090000
#endif
#include <Python.h>

extern "C" {
#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"
}

// -------------------- constants ------------------------------------
static const uint8_t FEATURES_STEP_SIZE = 10;
static const uint8_t PREPROCESSOR_FEATURE_SIZE = 40;
static const uint8_t FEATURE_DURATION_MS = 30;
static const uint16_t AUDIO_SAMPLE_FREQUENCY = 16000;
static const uint16_t SAMPLES_PER_CHUNK =
    FEATURES_STEP_SIZE * (AUDIO_SAMPLE_FREQUENCY / 1000);
// 16-bit mono
static const uint16_t BYTES_PER_CHUNK = SAMPLES_PER_CHUNK * 2;

#define FLOAT32_SCALE 0.0390625f

// -------------------- per-instance handle ----------------------------
struct FrontendHandle {
    FrontendConfig cfg;
    FrontendState st;
};

static const char *CAPSULE_NAME = "micro_features_cpp.FrontendHandle";

// -------------------- helpers --------------------
static inline void init_cfg(FrontendConfig *cfg) {
    cfg->window.size_ms = FEATURE_DURATION_MS;
    cfg->window.step_size_ms = FEATURES_STEP_SIZE;

    cfg->filterbank.num_channels = PREPROCESSOR_FEATURE_SIZE;
    cfg->filterbank.lower_band_limit = 125.0f;
    cfg->filterbank.upper_band_limit = 7500.0f;

    cfg->noise_reduction.smoothing_bits = 10;
    cfg->noise_reduction.even_smoothing = 0.025f;
    cfg->noise_reduction.odd_smoothing = 0.06f;
    cfg->noise_reduction.min_signal_remaining = 0.05f;

    cfg->pcan_gain_control.enable_pcan = 1;
    cfg->pcan_gain_control.strength = 0.95f;
    cfg->pcan_gain_control.offset = 80.0f;
    cfg->pcan_gain_control.gain_bits = 21;

    cfg->log_scale.enable_log = 1;
    cfg->log_scale.scale_shift = 6;
}

static void frontend_capsule_destructor(PyObject *capsule) {
    void *p = PyCapsule_GetPointer(capsule, CAPSULE_NAME);
    if (!p) {
        // Name mismatch or already invalid; do not raise from a destructor.
        PyErr_Clear();
        return;
    }
    auto *h = (FrontendHandle *)p;

    // Free TFLM state internals (noop if not needed by your version)
    FrontendFreeStateContents(&h->st);
    free(h);

    PyErr_Clear(); // ensure no lingering errors from any capsule calls
}

static inline FrontendHandle *get_handle(PyObject *cap) {
    return (FrontendHandle *)PyCapsule_GetPointer(cap, CAPSULE_NAME);
}

// -------------------- create_frontend() --------------------
static PyObject *mod_create_frontend(PyObject *, PyObject *) {
    auto *h = (FrontendHandle *)malloc(sizeof(FrontendHandle));
    if (!h)
        return PyErr_NoMemory();

    init_cfg(&h->cfg);
    FrontendPopulateState(&h->cfg, &h->st, AUDIO_SAMPLE_FREQUENCY);

    PyObject *capsule =
        PyCapsule_New((void *)h, CAPSULE_NAME, frontend_capsule_destructor);
    if (!capsule) {
        FrontendFreeStateContents(&h->st);
        free(h);
        return NULL;
    }
    return capsule;
}

// -------------------- process_samples(frontend, audio) --------------------
static PyObject *mod_process_samples(PyObject *, PyObject *args,
                                     PyObject *kwargs) {
    static const char *kwlist[] = {"frontend", "audio", NULL};
    PyObject *cap = NULL;
    const char *data = nullptr;
    Py_ssize_t len = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Oy#", (char **)kwlist, &cap,
                                     &data, &len)) {
        return NULL;
    }

    FrontendHandle *h = get_handle(cap);
    if (!h) {
        // PyCapsule_GetPointer already set an error (name mismatch or NULL)
        return NULL;
    }

    if (len < (Py_ssize_t)BYTES_PER_CHUNK) {
        PyErr_Format(
            PyExc_ValueError,
            "audio length (%zd bytes) < required chunk size (%u bytes)",
            (Py_ssize_t)len, (unsigned)BYTES_PER_CHUNK);
        return NULL;
    }

    const int16_t *samples = (const int16_t *)data;

    size_t samples_read = 0;
    FrontendOutput fo;
    Py_BEGIN_ALLOW_THREADS fo = FrontendProcessSamples(
        &h->st, samples, SAMPLES_PER_CHUNK, &samples_read);
    Py_END_ALLOW_THREADS

        PyObject *features_list = PyList_New((Py_ssize_t)fo.size);
    if (!features_list)
        return NULL;

    for (size_t i = 0; i < fo.size; ++i) {
        double v = (double)(fo.values[i] * FLOAT32_SCALE);
        PyObject *f = PyFloat_FromDouble(v);
        if (!f) {
            Py_DECREF(features_list);
            return NULL;
        }
        if (PyList_SetItem(features_list, (Py_ssize_t)i, f) < 0) {
            Py_DECREF(f);
            Py_DECREF(features_list);
            return NULL;
        }
    }

    PyObject *py_samples_read = PyLong_FromSize_t(samples_read);
    if (!py_samples_read) {
        Py_DECREF(features_list);
        return NULL;
    }

    PyObject *ret = PyTuple_New(2);
    if (!ret) {
        Py_DECREF(py_samples_read);
        Py_DECREF(features_list);
        return NULL;
    }
    if (PyTuple_SetItem(ret, 0, features_list) < 0) {
        Py_DECREF(ret); // also DECREFs features_list via failure path
        Py_DECREF(py_samples_read);
        return NULL;
    }
    if (PyTuple_SetItem(ret, 1, py_samples_read) < 0) {
        Py_DECREF(ret);
        return NULL;
    }

    return ret;
}

// -------------------- reset_frontend(frontend) --------------------
static PyObject *mod_reset_frontend(PyObject *, PyObject *args) {
    PyObject *cap = NULL;
    if (!PyArg_ParseTuple(args, "O", &cap))
        return NULL;

    FrontendHandle *h = get_handle(cap);
    if (!h)
        return NULL;

    FrontendFreeStateContents(&h->st);
    FrontendPopulateState(&h->cfg, &h->st, AUDIO_SAMPLE_FREQUENCY);

    Py_RETURN_NONE;
}

// -------------------- module boilerplate --------------------
static PyMethodDef module_methods[] = {
    {"create_frontend", (PyCFunction)mod_create_frontend, METH_NOARGS,
     "create_frontend() -> capsule"},
    {"process_samples", (PyCFunction)mod_process_samples,
     METH_VARARGS | METH_KEYWORDS,
     "process_samples(frontend, audio: bytes) -> (features: list[float], "
     "samples_read: int)"},
    {"reset_frontend", (PyCFunction)mod_reset_frontend, METH_VARARGS,
     "reset_frontend(frontend) -> None"},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "micro_features_cpp",
    "Speech features using TFLite Micro audio frontend.",
    -1,
    module_methods,
    NULL,
    NULL,
    NULL,
    NULL};

PyMODINIT_FUNC PyInit_micro_features_cpp(void) {
    PyObject *m = PyModule_Create(&module_def);
    if (!m)
        return NULL;

#ifdef VERSION_INFO
    PyObject *ver = PyUnicode_FromString(VERSION_INFO);
#else
    PyObject *ver = PyUnicode_FromString("dev");
#endif
    if (ver) {
        PyModule_AddObject(m, "__version__", ver); // steals ref
    }
    return m;
}
