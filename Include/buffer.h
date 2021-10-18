/* Public Py_buffer API */

#ifndef Py_BUFFER_H
#define Py_BUFFER_H
#ifdef __cplusplus
extern "C" {
#endif

/* === New Buffer API ============================================
 * Limited API since Python 3.11
 */

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x030b0000

typedef struct bufferinfo Py_buffer;

/* Return 1 if the getbuffer function is available, otherwise return 0. */
PyAPI_FUNC(int) PyObject_CheckBuffer(PyObject *obj);

/* This is a C-API version of the getbuffer function call.  It checks
   to make sure object has the required function pointer and issues the
   call.

   Returns -1 and raises an error on failure and returns 0 on success. */
PyAPI_FUNC(int) PyObject_GetBuffer(PyObject *obj, Py_buffer *view,
                                   int flags);

/* Get the memory area pointed to by the indices for the buffer given.
   Note that view->ndim is the assumed size of indices. */
PyAPI_FUNC(void *) PyBuffer_GetPointer(Py_buffer *view, Py_ssize_t *indices);

/* Return the implied itemsize of the data-format area from a
   struct-style description. */
PyAPI_FUNC(Py_ssize_t) PyBuffer_SizeFromFormat(const char *format);

/* Implementation in memoryobject.c */
PyAPI_FUNC(int) PyBuffer_ToContiguous(void *buf, Py_buffer *view,
                                      Py_ssize_t len, char order);

PyAPI_FUNC(int) PyBuffer_FromContiguous(Py_buffer *view, void *buf,
                                        Py_ssize_t len, char order);

/* Copy len bytes of data from the contiguous chunk of memory
   pointed to by buf into the buffer exported by obj.  Return
   0 on success and return -1 and raise a PyBuffer_Error on
   error (i.e. the object does not have a buffer interface or
   it is not working).

   If fort is 'F', then if the object is multi-dimensional,
   then the data will be copied into the array in
   Fortran-style (first dimension varies the fastest).  If
   fort is 'C', then the data will be copied into the array
   in C-style (last dimension varies the fastest).  If fort
   is 'A', then it does not matter and the copy will be made
   in whatever way is more efficient. */
PyAPI_FUNC(int) PyObject_CopyData(PyObject *dest, PyObject *src);

/* Copy the data from the src buffer to the buffer of destination. */
PyAPI_FUNC(int) PyBuffer_IsContiguous(const Py_buffer *view, char fort);

/*Fill the strides array with byte-strides of a contiguous
  (Fortran-style if fort is 'F' or C-style otherwise)
  array of the given shape with the given number of bytes
  per element. */
PyAPI_FUNC(void) PyBuffer_FillContiguousStrides(int ndims,
                                               Py_ssize_t *shape,
                                               Py_ssize_t *strides,
                                               int itemsize,
                                               char fort);

/* Fills in a buffer-info structure correctly for an exporter
   that can only share a contiguous chunk of memory of
   "unsigned bytes" of the given length.

   Returns 0 on success and -1 (with raising an error) on error. */
PyAPI_FUNC(int) PyBuffer_FillInfo(Py_buffer *view, PyObject *o, void *buf,
                                  Py_ssize_t len, int readonly,
                                  int flags);

PyAPI_FUNC(int) PyBuffer_FillInfoEx(Py_buffer *view, PyObject *obj, void *buf,
                                    Py_ssize_t len, int readonly, int flags,
                                    Py_ssize_t itemsize, int ndim,
                                    char *format, Py_ssize_t *shape,
                                    Py_ssize_t *strides, Py_ssize_t *suboffsets,
                                    void *internal);

/* Allocate a new buffer struct on the heap. */
PyAPI_FUNC(Py_buffer *) PyBuffer_New(void);

/* Release and free buffer struct which has been allocated
    by PyBuffer_New(). */
PyAPI_FUNC(void) PyBuffer_Free(Py_buffer *view);

/* Releases a Py_buffer obtained from getbuffer ParseTuple's "s*". */
PyAPI_FUNC(void) PyBuffer_Release(Py_buffer *view);

PyAPI_FUNC(PyObject *) PyBuffer_GetObject(Py_buffer *view);
PyAPI_FUNC(Py_ssize_t) PyBuffer_GetLength(Py_buffer *view);
PyAPI_FUNC(Py_ssize_t) PyBuffer_GetItemSize(Py_buffer *view);
PyAPI_FUNC(int) PyBuffer_IsReadonly(Py_buffer *view);
PyAPI_FUNC(void *) PyBuffer_GetInternal(Py_buffer *view);
PyAPI_FUNC(int) PyBuffer_GetLayout(Py_buffer *view, char **format,
                                   Py_ssize_t **shape, Py_ssize_t **strides,
                                   Py_ssize_t **suboffsets);

/* Maximum number of dimensions */
#define PyBUF_MAX_NDIM 64

/* Flags for getting buffers */
#define PyBUF_SIMPLE 0
#define PyBUF_WRITABLE 0x0001

#ifndef Py_LIMITED_API
/*  we used to include an E, backwards compatible alias */
#define PyBUF_WRITEABLE PyBUF_WRITABLE
#endif

#define PyBUF_FORMAT 0x0004
#define PyBUF_ND 0x0008
#define PyBUF_STRIDES (0x0010 | PyBUF_ND)
#define PyBUF_C_CONTIGUOUS (0x0020 | PyBUF_STRIDES)
#define PyBUF_F_CONTIGUOUS (0x0040 | PyBUF_STRIDES)
#define PyBUF_ANY_CONTIGUOUS (0x0080 | PyBUF_STRIDES)
#define PyBUF_INDIRECT (0x0100 | PyBUF_STRIDES)

#define PyBUF_CONTIG (PyBUF_ND | PyBUF_WRITABLE)
#define PyBUF_CONTIG_RO (PyBUF_ND)

#define PyBUF_STRIDED (PyBUF_STRIDES | PyBUF_WRITABLE)
#define PyBUF_STRIDED_RO (PyBUF_STRIDES)

#define PyBUF_RECORDS (PyBUF_STRIDES | PyBUF_WRITABLE | PyBUF_FORMAT)
#define PyBUF_RECORDS_RO (PyBUF_STRIDES | PyBUF_FORMAT)

#define PyBUF_FULL (PyBUF_INDIRECT | PyBUF_WRITABLE | PyBUF_FORMAT)
#define PyBUF_FULL_RO (PyBUF_INDIRECT | PyBUF_FORMAT)


#define PyBUF_READ  0x100
#define PyBUF_WRITE 0x200

#endif /* Py_LIMITED_API */

#ifdef __cplusplus
}
#endif
#endif /* Py_BUFFER_H */