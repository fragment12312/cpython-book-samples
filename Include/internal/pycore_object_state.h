#ifndef Py_INTERNAL_OBJECT_H
#define Py_INTERNAL_OBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

struct _py_object_runtime_state {
#ifdef Py_REF_DEBUG
    Py_ssize_t reftotal;
#endif
};


#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_OBJECT_H */
