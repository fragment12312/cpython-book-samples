/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_abstract.h"      // _PyNumber_Index()
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

static PyObject *
PyCField_new_impl(PyTypeObject *type, PyObject *name, Py_ssize_t size,
                  Py_ssize_t offset, Py_ssize_t bit_size);

static PyObject *
PyCField_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 4
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_item = { &_Py_ID(name), &_Py_ID(size), &_Py_ID(offset), &_Py_ID(bit_size), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"name", "size", "offset", "bit_size", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "CField",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[4];
    PyObject * const *fastargs;
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    Py_ssize_t noptargs = nargs + (kwargs ? PyDict_GET_SIZE(kwargs) : 0) - 3;
    PyObject *name;
    Py_ssize_t size;
    Py_ssize_t offset;
    Py_ssize_t bit_size = -1;

    fastargs = _PyArg_UnpackKeywords(_PyTuple_CAST(args)->ob_item, nargs, kwargs, NULL, &_parser, 3, 4, 0, argsbuf);
    if (!fastargs) {
        goto exit;
    }
    if (!PyUnicode_Check(fastargs[0])) {
        _PyArg_BadArgument("CField", "argument 'name'", "str", fastargs[0]);
        goto exit;
    }
    name = fastargs[0];
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(fastargs[1]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        size = ival;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(fastargs[2]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        offset = ival;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(fastargs[3]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        bit_size = ival;
    }
skip_optional_pos:
    return_value = PyCField_new_impl(type, name, size, offset, bit_size);

exit:
    return return_value;
}
/*[clinic end generated code: output=8e63c9e14920d7d4 input=a9049054013a1b77]*/
