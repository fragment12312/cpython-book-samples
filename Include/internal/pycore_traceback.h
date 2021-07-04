#ifndef Py_INTERNAL_TRACEBACK_H
#define Py_INTERNAL_TRACEBACK_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

/* Forward declaration */
struct _is;

/* Write the Python traceback into the file 'fd'. For example:

       Traceback (most recent call first):
         File "xxx", line xxx in <xxx>
         File "xxx", line xxx in <xxx>
         ...
         File "xxx", line xxx in <xxx>

   This function is written for debug purpose only, to dump the traceback in
   the worst case: after a segmentation fault, at fatal error, etc. That's why,
   it is very limited. Strings are truncated to 100 characters and encoded to
   ASCII with backslashreplace. It doesn't write the source code, only the
   function name, filename and line number of each frame. Write only the first
   100 frames: if the traceback is truncated, write the line " ...".

   This function is signal safe. */

PyAPI_FUNC(void) _Py_DumpTraceback(
    int fd,
    PyThreadState *tstate);

/* Write the traceback of all threads into the file 'fd'. current_thread can be
   NULL.

   Return NULL on success, or an error message on error.

   This function is written for debug purpose only. It calls
   _Py_DumpTraceback() for each thread, and so has the same limitations. It
   only write the traceback of the first 100 threads: write "..." if there are
   more threads.

   If current_tstate is NULL, the function tries to get the Python thread state
   of the current thread. It is not an error if the function is unable to get
   the current Python thread state.

   If interp is NULL, the function tries to get the interpreter state from
   the current Python thread state, or from
   _PyGILState_GetInterpreterStateUnsafe() in last resort.

   It is better to pass NULL to interp and current_tstate, the function tries
   different options to retrieve these informations.

   This function is signal safe. */

PyAPI_FUNC(const char*) _Py_DumpTracebackThreads(
    int fd,
    struct _is *interp,
    PyThreadState *current_tstate);

/* Write a Unicode object into the file descriptor fd. Encode the string to
   ASCII using the backslashreplace error handler.

   Do nothing if text is not a Unicode object. The function accepts Unicode
   string which is not ready (PyUnicode_WCHAR_KIND).

   This function is signal safe. */
PyAPI_FUNC(void) _Py_DumpASCII(int fd, PyObject *text);

/* Format an integer as decimal into the file descriptor fd.

   This function is signal safe. */
PyAPI_FUNC(void) _Py_DumpDecimal(
    int fd,
    size_t value);

/* Format an integer as hexadecimal with width digits into fd file descriptor.
   The function is signal safe. */
PyAPI_FUNC(void) _Py_DumpHexadecimal(
    int fd,
    uintptr_t value,
    Py_ssize_t width);

PyAPI_FUNC(PyObject*) _PyTraceBack_FromFrame(
    PyObject *tb_next,
    PyFrameObject *frame);

static inline Py_ssize_t
_byte_offset_to_character_offset(PyObject *line, Py_ssize_t col_offset)
{
    const char *str = PyUnicode_AsUTF8(line);
    if (!str) {
        return 0;
    }
    Py_ssize_t len = strlen(str);
    if (col_offset > len + 1) {
        col_offset = len + 1;
    }
    assert(col_offset >= 0);
    PyObject *text = PyUnicode_DecodeUTF8(str, col_offset, "replace");
    if (!text) {
        return 0;
    }
    Py_ssize_t size = PyUnicode_GET_LENGTH(text);
    Py_DECREF(text);
    return size;
}

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_TRACEBACK_H */
