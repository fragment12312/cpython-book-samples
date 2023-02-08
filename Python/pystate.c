
/* Thread and interpreter state structures and their interfaces */

#include "Python.h"
#include "pycore_ceval.h"
#include "pycore_code.h"           // stats
#include "pycore_frame.h"
#include "pycore_initconfig.h"
#include "pycore_object.h"        // _PyType_InitCache()
#include "pycore_pyerrors.h"
#include "pycore_pylifecycle.h"
#include "pycore_pymem.h"         // _PyMem_SetDefaultAllocator()
#include "pycore_pystate.h"
#include "pycore_runtime_init.h"  // _PyRuntimeState_INIT
#include "pycore_sysmodule.h"

/* --------------------------------------------------------------------------
CAUTION

Always use PyMem_RawMalloc() and PyMem_RawFree() directly in this file.  A
number of these functions are advertised as safe to call when the GIL isn't
held, and in a debug build Python redirects (e.g.) PyMem_NEW (etc) to Python's
debugging obmalloc functions.  Those aren't thread-safe (they rely on the GIL
to avoid the expense of doing their own locking).
-------------------------------------------------------------------------- */

#ifdef HAVE_DLOPEN
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#if !HAVE_DECL_RTLD_LAZY
#define RTLD_LAZY 1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


/****************************************/
/* helpers for the current thread state */
/****************************************/

// API for the current thread state is further down.

/* "current" means one of:
   - bound to the current OS thread
   - holds the GIL
 */

//-------------------------------------------------
// a highly efficient lookup for the current thread
//-------------------------------------------------

/*
   The stored thread state is set by PyThreadState_Swap().

   For each of these functions, the GIL must be held by the current thread.
 */

static inline PyThreadState *
current_fast_get(_PyRuntimeState *runtime)
{
    return (PyThreadState*)_Py_atomic_load_relaxed(&runtime->tstate_current);
}

static inline void
current_fast_set(_PyRuntimeState *runtime, PyThreadState *tstate)
{
    assert(tstate != NULL);
    _Py_atomic_store_relaxed(&runtime->tstate_current, (uintptr_t)tstate);
}

static inline void
current_fast_clear(_PyRuntimeState *runtime)
{
    _Py_atomic_store_relaxed(&runtime->tstate_current, (uintptr_t)NULL);
}

#define tstate_verify_not_active(tstate) \
    if (tstate == current_fast_get((tstate)->interp->runtime)) { \
        _Py_FatalErrorFormat(__func__, "tstate %p is still current", tstate); \
    }


//------------------------------------------------
// the thread state bound to the current OS thread
//------------------------------------------------

static inline int
tstate_tss_initialized(Py_tss_t *key)
{
    return PyThread_tss_is_created(key);
}

static inline int
tstate_tss_init(Py_tss_t *key)
{
    assert(!tstate_tss_initialized(key));
    return PyThread_tss_create(key);
}

static inline void
tstate_tss_fini(Py_tss_t *key)
{
    assert(tstate_tss_initialized(key));
    PyThread_tss_delete(key);
}

static inline PyThreadState *
tstate_tss_get(Py_tss_t *key)
{
    assert(tstate_tss_initialized(key));
    return (PyThreadState *)PyThread_tss_get(key);
}

static inline int
tstate_tss_set(Py_tss_t *key, PyThreadState *tstate)
{
    assert(tstate != NULL);
    assert(tstate_tss_initialized(key));
    return PyThread_tss_set(key, (void *)tstate);
}

static inline int
tstate_tss_clear(Py_tss_t *key)
{
    assert(tstate_tss_initialized(key));
    return PyThread_tss_set(key, (void *)NULL);
}

#ifdef HAVE_FORK
/* Reset the TSS key - called by PyOS_AfterFork_Child().
 * This should not be necessary, but some - buggy - pthread implementations
 * don't reset TSS upon fork(), see issue #10517.
 */
static PyStatus
tstate_tss_reinit(Py_tss_t *key)
{
    if (!tstate_tss_initialized(key)) {
        return _PyStatus_OK();
    }
    PyThreadState *tstate = tstate_tss_get(key);

    tstate_tss_fini(key);
    if (tstate_tss_init(key) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    /* If the thread had an associated auto thread state, reassociate it with
     * the new key. */
    if (tstate && tstate_tss_set(key, tstate) != 0) {
        return _PyStatus_ERR("failed to re-set autoTSSkey");
    }
    return _PyStatus_OK();
}
#endif


/*
   The stored thread state is set by bind_tstate() (AKA PyThreadState_Bind().

   The GIL does no need to be held for these.
  */

#define gilstate_tss_initialized(runtime) \
    tstate_tss_initialized(&(runtime)->autoTSSkey)
#define gilstate_tss_init(runtime) \
    tstate_tss_init(&(runtime)->autoTSSkey)
#define gilstate_tss_fini(runtime) \
    tstate_tss_fini(&(runtime)->autoTSSkey)
#define gilstate_tss_get(runtime) \
    tstate_tss_get(&(runtime)->autoTSSkey)
#define _gilstate_tss_set(runtime, tstate) \
    tstate_tss_set(&(runtime)->autoTSSkey, tstate)
#define _gilstate_tss_clear(runtime) \
    tstate_tss_clear(&(runtime)->autoTSSkey)
#define gilstate_tss_reinit(runtime) \
    tstate_tss_reinit(&(runtime)->autoTSSkey)

static inline void
gilstate_tss_set(_PyRuntimeState *runtime, PyThreadState *tstate)
{
    assert(tstate != NULL && tstate->interp->runtime == runtime);
    if (_gilstate_tss_set(runtime, tstate) != 0) {
        Py_FatalError("failed to set current tstate (TSS)");
    }
}

static inline void
gilstate_tss_clear(_PyRuntimeState *runtime)
{
    if (_gilstate_tss_clear(runtime) != 0) {
        Py_FatalError("failed to clear current tstate (TSS)");
    }
}


static inline int tstate_is_alive(PyThreadState *tstate);

static inline int
tstate_is_bound(PyThreadState *tstate)
{
    return tstate->_status.bound && !tstate->_status.unbound;
}

static void bind_gilstate_tstate(PyThreadState *);
static void unbind_gilstate_tstate(PyThreadState *);

static void
bind_tstate(PyThreadState *tstate)
{
    assert(tstate != NULL);
    assert(tstate_is_alive(tstate) && !tstate->_status.bound);
    assert(!tstate->_status.unbound);  // just in case
    assert(!tstate->_status.bound_gilstate);
    assert(tstate != gilstate_tss_get(tstate->interp->runtime));
    assert(!tstate->_status.active);
    assert(tstate->thread_id == 0);
    assert(tstate->native_thread_id == 0);

    // Currently we don't necessarily store the thread state
    // in thread-local storage (e.g. per-interpreter).

    tstate->thread_id = PyThread_get_thread_ident();
#ifdef PY_HAVE_THREAD_NATIVE_ID
    tstate->native_thread_id = PyThread_get_thread_native_id();
#endif

    tstate->_status.bound = 1;
}

static void
unbind_tstate(PyThreadState *tstate)
{
    assert(tstate != NULL);
    // XXX assert(tstate_is_alive(tstate));
    assert(tstate_is_bound(tstate));
    // XXX assert(!tstate->_status.active);
    assert(tstate->thread_id > 0);
#ifdef PY_HAVE_THREAD_NATIVE_ID
    assert(tstate->native_thread_id > 0);
#endif

    // We leave thread_id and native_thread_id alone
    // since they can be useful for debugging.
    // Check the `_status` field to know if these values
    // are still valid.

    // We leave tstate->_status.bound set to 1
    // to indicate it was previously bound.
    tstate->_status.unbound = 1;
}


/* Stick the thread state for this thread in thread specific storage.

   When a thread state is created for a thread by some mechanism
   other than PyGILState_Ensure(), it's important that the GILState
   machinery knows about it so it doesn't try to create another
   thread state for the thread.
   (This is a better fix for SF bug #1010677 than the first one attempted.)

   The only situation where you can legitimately have more than one
   thread state for an OS level thread is when there are multiple
   interpreters.

   Before 3.12, the PyGILState_*() APIs didn't work with multiple
   interpreters (see bpo-10915 and bpo-15751), so this function used
   to set TSS only once.  Thus, the first thread state created for that
   given OS level thread would "win", which seemed reasonable behaviour.
*/

static void
bind_gilstate_tstate(PyThreadState *tstate)
{
    assert(tstate != NULL);
    assert(tstate_is_alive(tstate));
    assert(tstate_is_bound(tstate));
    // XXX assert(!tstate->_status.active);
    assert(!tstate->_status.bound_gilstate);

    _PyRuntimeState *runtime = tstate->interp->runtime;
    PyThreadState *tcur = gilstate_tss_get(runtime);
    assert(tstate != tcur);

    if (tcur != NULL) {
        tcur->_status.bound_gilstate = 0;
    }
    gilstate_tss_set(runtime, tstate);
    tstate->_status.bound_gilstate = 1;
}

static void
unbind_gilstate_tstate(PyThreadState *tstate)
{
    assert(tstate != NULL);
    // XXX assert(tstate_is_alive(tstate));
    assert(tstate_is_bound(tstate));
    // XXX assert(!tstate->_status.active);
    assert(tstate->_status.bound_gilstate);
    assert(tstate == gilstate_tss_get(tstate->interp->runtime));

    gilstate_tss_clear(tstate->interp->runtime);
    tstate->_status.bound_gilstate = 0;
}


//----------------------------------------------
// the thread state that currently holds the GIL
//----------------------------------------------

/* This is not exported, as it is not reliable!  It can only
   ever be compared to the state for the *current* thread.
   * If not equal, then it doesn't matter that the actual
     value may change immediately after comparison, as it can't
     possibly change to the current thread's state.
   * If equal, then the current thread holds the lock, so the value can't
     change until we yield the lock.
*/
static int
holds_gil(PyThreadState *tstate)
{
    // XXX Fall back to tstate->interp->runtime->ceval.gil.last_holder
    // (and tstate->interp->runtime->ceval.gil.locked).
    assert(tstate != NULL);
    _PyRuntimeState *runtime = tstate->interp->runtime;
    /* Must be the tstate for this thread */
    assert(tstate == gilstate_tss_get(runtime));
    return tstate == current_fast_get(runtime);
}


/****************************/
/* the global runtime state */
/****************************/

//----------
// lifecycle
//----------

/* Suppress deprecation warning for PyBytesObject.ob_shash */
_Py_COMP_DIAG_PUSH
_Py_COMP_DIAG_IGNORE_DEPR_DECLS
/* We use "initial" if the runtime gets re-used
   (e.g. Py_Finalize() followed by Py_Initialize().
   Note that we initialize "initial" relative to _PyRuntime,
   to ensure pre-initialized pointers point to the active
   runtime state (and not "initial"). */
static const _PyRuntimeState initial = _PyRuntimeState_INIT(_PyRuntime);
_Py_COMP_DIAG_POP

static int
alloc_for_runtime(PyThread_type_lock *plock1, PyThread_type_lock *plock2,
                  PyThread_type_lock *plock3, PyThread_type_lock *plock4)
{
    /* Force default allocator, since _PyRuntimeState_Fini() must
       use the same allocator than this function. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    PyThread_type_lock lock1 = PyThread_allocate_lock();
    if (lock1 == NULL) {
        return -1;
    }

    PyThread_type_lock lock2 = PyThread_allocate_lock();
    if (lock2 == NULL) {
        PyThread_free_lock(lock1);
        return -1;
    }

    PyThread_type_lock lock3 = PyThread_allocate_lock();
    if (lock3 == NULL) {
        PyThread_free_lock(lock1);
        PyThread_free_lock(lock2);
        return -1;
    }

    PyThread_type_lock lock4 = PyThread_allocate_lock();
    if (lock4 == NULL) {
        PyThread_free_lock(lock1);
        PyThread_free_lock(lock2);
        PyThread_free_lock(lock3);
        return -1;
    }

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    *plock1 = lock1;
    *plock2 = lock2;
    *plock3 = lock3;
    *plock4 = lock4;
    return 0;
}

static void
init_runtime(_PyRuntimeState *runtime,
             void *open_code_hook, void *open_code_userdata,
             _Py_AuditHookEntry *audit_hook_head,
             Py_ssize_t unicode_next_index,
             PyThread_type_lock unicode_ids_mutex,
             PyThread_type_lock interpreters_mutex,
             PyThread_type_lock xidregistry_mutex,
             PyThread_type_lock getargs_mutex)
{
    if (runtime->_initialized) {
        Py_FatalError("runtime already initialized");
    }
    assert(!runtime->preinitializing &&
           !runtime->preinitialized &&
           !runtime->core_initialized &&
           !runtime->initialized);

    runtime->open_code_hook = open_code_hook;
    runtime->open_code_userdata = open_code_userdata;
    runtime->audit_hook_head = audit_hook_head;

    _PyEval_InitRuntimeState(&runtime->ceval);

    PyPreConfig_InitPythonConfig(&runtime->preconfig);

    runtime->interpreters.mutex = interpreters_mutex;

    runtime->xidregistry.mutex = xidregistry_mutex;

    runtime->getargs.mutex = getargs_mutex;

    // Set it to the ID of the main thread of the main interpreter.
    runtime->main_thread = PyThread_get_thread_ident();

    runtime->unicode_state.ids.next_index = unicode_next_index;
    runtime->unicode_state.ids.lock = unicode_ids_mutex;

    runtime->_initialized = 1;
}

PyStatus
_PyRuntimeState_Init(_PyRuntimeState *runtime)
{
    /* We preserve the hook across init, because there is
       currently no public API to set it between runtime
       initialization and interpreter initialization. */
    void *open_code_hook = runtime->open_code_hook;
    void *open_code_userdata = runtime->open_code_userdata;
    _Py_AuditHookEntry *audit_hook_head = runtime->audit_hook_head;
    // bpo-42882: Preserve next_index value if Py_Initialize()/Py_Finalize()
    // is called multiple times.
    Py_ssize_t unicode_next_index = runtime->unicode_state.ids.next_index;

    PyThread_type_lock lock1, lock2, lock3, lock4;
    if (alloc_for_runtime(&lock1, &lock2, &lock3, &lock4) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    if (runtime->_initialized) {
        // Py_Initialize() must be running again.
        // Reset to _PyRuntimeState_INIT.
        memcpy(runtime, &initial, sizeof(*runtime));
    }

    if (gilstate_tss_init(runtime) != 0) {
        _PyRuntimeState_Fini(runtime);
        return _PyStatus_NO_MEMORY();
    }

    if (PyThread_tss_create(&runtime->trashTSSkey) != 0) {
        _PyRuntimeState_Fini(runtime);
        return _PyStatus_NO_MEMORY();
    }

    init_runtime(runtime, open_code_hook, open_code_userdata, audit_hook_head,
                 unicode_next_index, lock1, lock2, lock3, lock4);

    return _PyStatus_OK();
}

void
_PyRuntimeState_Fini(_PyRuntimeState *runtime)
{
    if (gilstate_tss_initialized(runtime)) {
        gilstate_tss_fini(runtime);
    }

    if (PyThread_tss_is_created(&runtime->trashTSSkey)) {
        PyThread_tss_delete(&runtime->trashTSSkey);
    }

    /* Force the allocator used by _PyRuntimeState_Init(). */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
#define FREE_LOCK(LOCK) \
    if (LOCK != NULL) { \
        PyThread_free_lock(LOCK); \
        LOCK = NULL; \
    }

    FREE_LOCK(runtime->interpreters.mutex);
    FREE_LOCK(runtime->xidregistry.mutex);
    FREE_LOCK(runtime->unicode_state.ids.lock);
    FREE_LOCK(runtime->getargs.mutex);

#undef FREE_LOCK
    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);
}

#ifdef HAVE_FORK
/* This function is called from PyOS_AfterFork_Child to ensure that
   newly created child processes do not share locks with the parent. */
PyStatus
_PyRuntimeState_ReInitThreads(_PyRuntimeState *runtime)
{
    // This was initially set in _PyRuntimeState_Init().
    runtime->main_thread = PyThread_get_thread_ident();

    /* Force default allocator, since _PyRuntimeState_Fini() must
       use the same allocator than this function. */
    PyMemAllocatorEx old_alloc;
    _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    int reinit_interp = _PyThread_at_fork_reinit(&runtime->interpreters.mutex);
    int reinit_xidregistry = _PyThread_at_fork_reinit(&runtime->xidregistry.mutex);
    int reinit_unicode_ids = _PyThread_at_fork_reinit(&runtime->unicode_state.ids.lock);
    int reinit_getargs = _PyThread_at_fork_reinit(&runtime->getargs.mutex);

    PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

    /* bpo-42540: id_mutex is freed by _PyInterpreterState_Delete, which does
     * not force the default allocator. */
    int reinit_main_id = _PyThread_at_fork_reinit(&runtime->interpreters.main->id_mutex);

    if (reinit_interp < 0
        || reinit_main_id < 0
        || reinit_xidregistry < 0
        || reinit_unicode_ids < 0
        || reinit_getargs < 0)
    {
        return _PyStatus_ERR("Failed to reinitialize runtime locks");

    }

    PyStatus status = gilstate_tss_reinit(runtime);
    if (_PyStatus_EXCEPTION(status)) {
        return status;
    }

    if (PyThread_tss_is_created(&runtime->trashTSSkey)) {
        PyThread_tss_delete(&runtime->trashTSSkey);
    }
    if (PyThread_tss_create(&runtime->trashTSSkey) != 0) {
        return _PyStatus_NO_MEMORY();
    }

    return _PyStatus_OK();
}
#endif


/*************************************/
/* the per-interpreter runtime state */
/*************************************/

//----------
// lifecycle
//----------

/* Calling this indicates that the runtime is ready to create interpreters. */

PyStatus
_PyInterpreterState_Enable(_PyRuntimeState *runtime)
{
    struct pyinterpreters *interpreters = &runtime->interpreters;
    interpreters->next_id = 0;

    /* Py_Finalize() calls _PyRuntimeState_Fini() which clears the mutex.
       Create a new mutex if needed. */
    if (interpreters->mutex == NULL) {
        /* Force default allocator, since _PyRuntimeState_Fini() must
           use the same allocator than this function. */
        PyMemAllocatorEx old_alloc;
        _PyMem_SetDefaultAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

        interpreters->mutex = PyThread_allocate_lock();

        PyMem_SetAllocator(PYMEM_DOMAIN_RAW, &old_alloc);

        if (interpreters->mutex == NULL) {
            return _PyStatus_ERR("Can't initialize threads for interpreter");
        }
    }

    return _PyStatus_OK();
}


static PyInterpreterState *
alloc_interpreter(void)
{
    return PyMem_RawCalloc(1, sizeof(PyInterpreterState));
}

static void
free_interpreter(PyInterpreterState *interp)
{
    // The main interpreter is statically allocated so
    // should not be freed.
    if (interp != &_PyRuntime._main_interpreter) {
        PyMem_RawFree(interp);
    }
}

/* Get the interpreter state to a minimal consistent state.
   Further init happens in pylifecycle.c before it can be used.
   All fields not initialized here are expected to be zeroed out,
   e.g. by PyMem_RawCalloc() or memset(), or otherwise pre-initialized.
   The runtime state is not manipulated.  Instead it is assumed that
   the interpreter is getting added to the runtime.
  */

static void
init_interpreter(PyInterpreterState *interp,
                 _PyRuntimeState *runtime, int64_t id,
                 PyInterpreterState *next,
                 PyThread_type_lock pending_lock)
{
    if (interp->_initialized) {
        Py_FatalError("interpreter already initialized");
    }

    assert(runtime != NULL);
    interp->runtime = runtime;

    assert(id > 0 || (id == 0 && interp == runtime->interpreters.main));
    interp->id = id;

    assert(runtime->interpreters.head == interp);
    assert(next != NULL || (interp == runtime->interpreters.main));
    interp->next = next;

    _PyEval_InitState(&interp->ceval, pending_lock);
    _PyGC_InitState(&interp->gc);
    PyConfig_InitPythonConfig(&interp->config);
    _PyType_InitCache(interp);

    interp->_initialized = 1;
}

PyInterpreterState *
PyInterpreterState_New(void)
{
    PyInterpreterState *interp;
    _PyRuntimeState *runtime = &_PyRuntime;
    PyThreadState *tstate = current_fast_get(runtime);

    /* tstate is NULL when Py_InitializeFromConfig() calls
       PyInterpreterState_New() to create the main interpreter. */
    if (_PySys_Audit(tstate, "cpython.PyInterpreterState_New", NULL) < 0) {
        return NULL;
    }

    PyThread_type_lock pending_lock = PyThread_allocate_lock();
    if (pending_lock == NULL) {
        if (tstate != NULL) {
            _PyErr_NoMemory(tstate);
        }
        return NULL;
    }

    /* Don't get runtime from tstate since tstate can be NULL. */
    struct pyinterpreters *interpreters = &runtime->interpreters;

    /* We completely serialize creation of multiple interpreters, since
       it simplifies things here and blocking concurrent calls isn't a problem.
       Regardless, we must fully block subinterpreter creation until
       after the main interpreter is created. */
    HEAD_LOCK(runtime);

    int64_t id = interpreters->next_id;
    interpreters->next_id += 1;

    // Allocate the interpreter and add it to the runtime state.
    PyInterpreterState *old_head = interpreters->head;
    if (old_head == NULL) {
        // We are creating the main interpreter.
        assert(interpreters->main == NULL);
        assert(id == 0);

        interp = &runtime->_main_interpreter;
        assert(interp->id == 0);
        assert(interp->next == NULL);

        interpreters->main = interp;
    }
    else {
        assert(interpreters->main != NULL);
        assert(id != 0);

        interp = alloc_interpreter();
        if (interp == NULL) {
            goto error;
        }
        // Set to _PyInterpreterState_INIT.
        memcpy(interp, &initial._main_interpreter,
               sizeof(*interp));

        if (id < 0) {
            /* overflow or Py_Initialize() not called yet! */
            if (tstate != NULL) {
                _PyErr_SetString(tstate, PyExc_RuntimeError,
                                 "failed to get an interpreter ID");
            }
            goto error;
        }
    }
    interpreters->head = interp;

    init_interpreter(interp, runtime, id, old_head, pending_lock);

    for (int i=0; i < INTERP_NUM_FREELISTS; i++) {
        _PyFreeList_Init(&interp->freelists[i],
                         FREELIST_INDEX_TO_ALLOCATED_SIZE(i),
                         SMALL_OBJECT_FREELIST_SIZE);
    }

    HEAD_UNLOCK(runtime);
    return interp;

error:
    HEAD_UNLOCK(runtime);

    PyThread_free_lock(pending_lock);
    if (interp != NULL) {
        free_interpreter(interp);
    }
    return NULL;
}


static void
interpreter_clear(PyInterpreterState *interp, PyThreadState *tstate)
{
    assert(interp != NULL);
    assert(tstate != NULL);
    _PyRuntimeState *runtime = interp->runtime;

    /* XXX Conditions we need to enforce:

       * the GIL must be held by the current thread
       * tstate must be the "current" thread state (current_fast_get())
       * tstate->interp must be interp
       * for the main interpreter, tstate must be the main thread
     */
    // XXX Ideally, we would not rely on any thread state in this function
    // (and we would drop the "tstate" argument).

    if (_PySys_Audit(tstate, "cpython.PyInterpreterState_Clear", NULL) < 0) {
        _PyErr_Clear(tstate);
    }

    HEAD_LOCK(runtime);
    // XXX Clear the current/main thread state last.
    for (PyThreadState *p = interp->threads.head; p != NULL; p = p->next) {
        PyThreadState_Clear(p);
    }
    HEAD_UNLOCK(runtime);

    for (int i=0; i < INTERP_NUM_FREELISTS; i++) {
        _PyFreeList_Clear(&interp->freelists[i]);
        interp->freelists[i].space = 0;
        interp->freelists[i].capacity = 0;
    }

    /* It is possible that any of the objects below have a finalizer
       that runs Python code or otherwise relies on a thread state
       or even the interpreter state.  For now we trust that isn't
       a problem.
     */
    // XXX Make sure we properly deal with problematic finalizers.

    Py_CLEAR(interp->audit_hooks);

    PyConfig_Clear(&interp->config);
    Py_CLEAR(interp->codec_search_path);
    Py_CLEAR(interp->codec_search_cache);
    Py_CLEAR(interp->codec_error_registry);
    Py_CLEAR(interp->modules);
    Py_CLEAR(interp->modules_by_index);
    Py_CLEAR(interp->builtins_copy);
    Py_CLEAR(interp->importlib);
    Py_CLEAR(interp->import_func);
    Py_CLEAR(interp->dict);
#ifdef HAVE_FORK
    Py_CLEAR(interp->before_forkers);
    Py_CLEAR(interp->after_forkers_parent);
    Py_CLEAR(interp->after_forkers_child);
#endif

    _PyAST_Fini(interp);
    _PyWarnings_Fini(interp);
    _PyAtExit_Fini(interp);

    // All Python types must be destroyed before the last GC collection. Python
    // types create a reference cycle to themselves in their in their
    // PyTypeObject.tp_mro member (the tuple contains the type).

    /* Last garbage collection on this interpreter */
    _PyGC_CollectNoFail(tstate);
    _PyGC_Fini(interp);

    /* We don't clear sysdict and builtins until the end of this function.
       Because clearing other attributes can execute arbitrary Python code
       which requires sysdict and builtins. */
    PyDict_Clear(interp->sysdict);
    PyDict_Clear(interp->builtins);
    Py_CLEAR(interp->sysdict);
    Py_CLEAR(interp->builtins);
    Py_CLEAR(interp->interpreter_trampoline);

    for (int i=0; i < DICT_MAX_WATCHERS; i++) {
        interp->dict_state.watchers[i] = NULL;
    }

    for (int i=0; i < TYPE_MAX_WATCHERS; i++) {
        interp->type_watchers[i] = NULL;
    }

    for (int i=0; i < FUNC_MAX_WATCHERS; i++) {
        interp->func_watchers[i] = NULL;
    }
    interp->active_func_watchers = 0;

    for (int i=0; i < CODE_MAX_WATCHERS; i++) {
        interp->code_watchers[i] = NULL;
    }
    interp->active_code_watchers = 0;

    // XXX Once we have one allocator per interpreter (i.e.
    // per-interpreter GC) we must ensure that all of the interpreter's
    // objects have been cleaned up at the point.
}


void
PyInterpreterState_Clear(PyInterpreterState *interp)
{
    // Use the current Python thread state to call audit hooks and to collect
    // garbage. It can be different than the current Python thread state
    // of 'interp'.
    PyThreadState *current_tstate = current_fast_get(interp->runtime);
    interpreter_clear(interp, current_tstate);
}


void
_PyInterpreterState_Clear(PyThreadState *tstate)
{
    interpreter_clear(tstate->interp, tstate);
}


static void zapthreads(PyInterpreterState *interp);

void
PyInterpreterState_Delete(PyInterpreterState *interp)
{
    _PyRuntimeState *runtime = interp->runtime;
    struct pyinterpreters *interpreters = &runtime->interpreters;

    // XXX Clearing the "current" thread state should happen before
    // we start finalizing the interpreter (or the current thread state).
    PyThreadState *tcur = current_fast_get(runtime);
    if (tcur != NULL && interp == tcur->interp) {
        /* Unset current thread.  After this, many C API calls become crashy. */
        _PyThreadState_Swap(runtime, NULL);
    }

    zapthreads(interp);

    _PyEval_FiniState(&interp->ceval);

    HEAD_LOCK(runtime);
    PyInterpreterState **p;
    for (p = &interpreters->head; ; p = &(*p)->next) {
        if (*p == NULL) {
            Py_FatalError("NULL interpreter");
        }
        if (*p == interp) {
            break;
        }
    }
    if (interp->threads.head != NULL) {
        Py_FatalError("remaining threads");
    }
    *p = interp->next;

    if (interpreters->main == interp) {
        interpreters->main = NULL;
        if (interpreters->head != NULL) {
            Py_FatalError("remaining subinterpreters");
        }
    }
    HEAD_UNLOCK(runtime);

    if (interp->id_mutex != NULL) {
        PyThread_free_lock(interp->id_mutex);
    }
    free_interpreter(interp);
}


#ifdef HAVE_FORK
/*
 * Delete all interpreter states except the main interpreter.  If there
 * is a current interpreter state, it *must* be the main interpreter.
 */
PyStatus
_PyInterpreterState_DeleteExceptMain(_PyRuntimeState *runtime)
{
    struct pyinterpreters *interpreters = &runtime->interpreters;

    PyThreadState *tstate = _PyThreadState_Swap(runtime, NULL);
    if (tstate != NULL && tstate->interp != interpreters->main) {
        return _PyStatus_ERR("not main interpreter");
    }

    HEAD_LOCK(runtime);
    PyInterpreterState *interp = interpreters->head;
    interpreters->head = NULL;
    while (interp != NULL) {
        if (interp == interpreters->main) {
            interpreters->main->next = NULL;
            interpreters->head = interp;
            interp = interp->next;
            continue;
        }

        // XXX Won't this fail since PyInterpreterState_Clear() requires
        // the "current" tstate to be set?
        PyInterpreterState_Clear(interp);  // XXX must activate?
        zapthreads(interp);
        if (interp->id_mutex != NULL) {
            PyThread_free_lock(interp->id_mutex);
        }
        PyInterpreterState *prev_interp = interp;
        interp = interp->next;
        free_interpreter(prev_interp);
    }
    HEAD_UNLOCK(runtime);

    if (interpreters->head == NULL) {
        return _PyStatus_ERR("missing main interpreter");
    }
    _PyThreadState_Swap(runtime, tstate);
    return _PyStatus_OK();
}
#endif


// Used by finalize_modules()
void
_PyInterpreterState_ClearModules(PyInterpreterState *interp)
{
    if (!interp->modules_by_index) {
        return;
    }

    Py_ssize_t i;
    for (i = 0; i < PyList_GET_SIZE(interp->modules_by_index); i++) {
        PyObject *m = PyList_GET_ITEM(interp->modules_by_index, i);
        if (PyModule_Check(m)) {
            /* cleanup the saved copy of module dicts */
            PyModuleDef *md = PyModule_GetDef(m);
            if (md) {
                Py_CLEAR(md->m_base.m_copy);
            }
        }
    }

    /* Setting modules_by_index to NULL could be dangerous, so we
       clear the list instead. */
    if (PyList_SetSlice(interp->modules_by_index,
                        0, PyList_GET_SIZE(interp->modules_by_index),
                        NULL)) {
        PyErr_WriteUnraisable(interp->modules_by_index);
    }
}


//----------
// accessors
//----------

int64_t
PyInterpreterState_GetID(PyInterpreterState *interp)
{
    if (interp == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "no interpreter provided");
        return -1;
    }
    return interp->id;
}


int
_PyInterpreterState_IDInitref(PyInterpreterState *interp)
{
    if (interp->id_mutex != NULL) {
        return 0;
    }
    interp->id_mutex = PyThread_allocate_lock();
    if (interp->id_mutex == NULL) {
        PyErr_SetString(PyExc_RuntimeError,
                        "failed to create init interpreter ID mutex");
        return -1;
    }
    interp->id_refcount = 0;
    return 0;
}


int
_PyInterpreterState_IDIncref(PyInterpreterState *interp)
{
    if (_PyInterpreterState_IDInitref(interp) < 0) {
        return -1;
    }

    PyThread_acquire_lock(interp->id_mutex, WAIT_LOCK);
    interp->id_refcount += 1;
    PyThread_release_lock(interp->id_mutex);
    return 0;
}


void
_PyInterpreterState_IDDecref(PyInterpreterState *interp)
{
    assert(interp->id_mutex != NULL);
    _PyRuntimeState *runtime = interp->runtime;

    PyThread_acquire_lock(interp->id_mutex, WAIT_LOCK);
    assert(interp->id_refcount != 0);
    interp->id_refcount -= 1;
    int64_t refcount = interp->id_refcount;
    PyThread_release_lock(interp->id_mutex);

    if (refcount == 0 && interp->requires_idref) {
        // XXX Using the "head" thread isn't strictly correct.
        PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
        // XXX Possible GILState issues?
        PyThreadState *save_tstate = _PyThreadState_Swap(runtime, tstate);
        Py_EndInterpreter(tstate);
        _PyThreadState_Swap(runtime, save_tstate);
    }
}

int
_PyInterpreterState_RequiresIDRef(PyInterpreterState *interp)
{
    return interp->requires_idref;
}

void
_PyInterpreterState_RequireIDRef(PyInterpreterState *interp, int required)
{
    interp->requires_idref = required ? 1 : 0;
}

PyObject *
_PyInterpreterState_GetMainModule(PyInterpreterState *interp)
{
    if (interp->modules == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "interpreter not initialized");
        return NULL;
    }
    return PyMapping_GetItemString(interp->modules, "__main__");
}

PyObject *
PyInterpreterState_GetDict(PyInterpreterState *interp)
{
    if (interp->dict == NULL) {
        interp->dict = PyDict_New();
        if (interp->dict == NULL) {
            PyErr_Clear();
        }
    }
    /* Returning NULL means no per-interpreter dict is available. */
    return interp->dict;
}


//-----------------------------
// look up an interpreter state
//-----------------------------

/* Return the interpreter associated with the current OS thread.

   The GIL must be held.
  */

PyInterpreterState *
PyInterpreterState_Get(void)
{
    PyThreadState *tstate = current_fast_get(&_PyRuntime);
    _Py_EnsureTstateNotNULL(tstate);
    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        Py_FatalError("no current interpreter");
    }
    return interp;
}


static PyInterpreterState *
interp_look_up_id(_PyRuntimeState *runtime, int64_t requested_id)
{
    PyInterpreterState *interp = runtime->interpreters.head;
    while (interp != NULL) {
        int64_t id = PyInterpreterState_GetID(interp);
        if (id < 0) {
            return NULL;
        }
        if (requested_id == id) {
            return interp;
        }
        interp = PyInterpreterState_Next(interp);
    }
    return NULL;
}

/* Return the interpreter state with the given ID.

   Fail with RuntimeError if the interpreter is not found. */

PyInterpreterState *
_PyInterpreterState_LookUpID(int64_t requested_id)
{
    PyInterpreterState *interp = NULL;
    if (requested_id >= 0) {
        _PyRuntimeState *runtime = &_PyRuntime;
        HEAD_LOCK(runtime);
        interp = interp_look_up_id(runtime, requested_id);
        HEAD_UNLOCK(runtime);
    }
    if (interp == NULL && !PyErr_Occurred()) {
        PyErr_Format(PyExc_RuntimeError,
                     "unrecognized interpreter ID %lld", requested_id);
    }
    return interp;
}


/********************************/
/* the per-thread runtime state */
/********************************/

static inline int
tstate_is_alive(PyThreadState *tstate)
{
    return (tstate->_status.initialized &&
            !tstate->_status.finalized &&
            !tstate->_status.cleared &&
            !tstate->_status.finalizing);
}


//----------
// lifecycle
//----------

/* Minimum size of data stack chunk */
#define DATA_STACK_CHUNK_SIZE (16*1024)

static _PyStackChunk*
allocate_chunk(int size_in_bytes, _PyStackChunk* previous)
{
    assert(size_in_bytes % sizeof(PyObject **) == 0);
    _PyStackChunk *res = _PyObject_VirtualAlloc(size_in_bytes);
    if (res == NULL) {
        return NULL;
    }
    res->previous = previous;
    res->size = size_in_bytes;
    res->top = 0;
    return res;
}

static PyThreadState *
alloc_threadstate(void)
{
    return PyMem_RawCalloc(1, sizeof(PyThreadState));
}

static void
free_threadstate(PyThreadState *tstate)
{
    // The initial thread state of the interpreter is allocated
    // as part of the interpreter state so should not be freed.
    if (tstate != &tstate->interp->_initial_thread) {
        PyMem_RawFree(tstate);
    }
}

/* Get the thread state to a minimal consistent state.
   Further init happens in pylifecycle.c before it can be used.
   All fields not initialized here are expected to be zeroed out,
   e.g. by PyMem_RawCalloc() or memset(), or otherwise pre-initialized.
   The interpreter state is not manipulated.  Instead it is assumed that
   the thread is getting added to the interpreter.
  */

static void
init_threadstate(PyThreadState *tstate,
                 PyInterpreterState *interp, uint64_t id,
                 PyThreadState *next)
{
    if (tstate->_status.initialized) {
        Py_FatalError("thread state already initialized");
    }

    assert(interp != NULL);
    tstate->interp = interp;

    assert(id > 0);
    tstate->id = id;

    assert(interp->threads.head == tstate);
    assert((next != NULL && id != 1) || (next == NULL && id == 1));
    if (next != NULL) {
        assert(next->prev == NULL || next->prev == tstate);
        next->prev = tstate;
    }
    tstate->next = next;
    assert(tstate->prev == NULL);

    // thread_id and native_thread_id are set in bind_tstate().

    tstate->py_recursion_limit = interp->ceval.recursion_limit,
    tstate->py_recursion_remaining = interp->ceval.recursion_limit,
    tstate->c_recursion_remaining = C_RECURSION_LIMIT;

    tstate->exc_info = &tstate->exc_state;

    // PyGILState_Release must not try to delete this thread state.
    // This is cleared when PyGILState_Ensure() creates the thread state.
    tstate->gilstate_counter = 1;

    tstate->cframe = &tstate->root_cframe;
    tstate->datastack_chunk = NULL;
    tstate->datastack_top = NULL;
    tstate->datastack_limit = NULL;

    tstate->_status.initialized = 1;
}

static PyThreadState *
new_threadstate(PyInterpreterState *interp)
{
    PyThreadState *tstate;
    _PyRuntimeState *runtime = interp->runtime;
    // We don't need to allocate a thread state for the main interpreter
    // (the common case), but doing it later for the other case revealed a
    // reentrancy problem (deadlock).  So for now we always allocate before
    // taking the interpreters lock.  See GH-96071.
    PyThreadState *new_tstate = alloc_threadstate();
    int used_newtstate;
    if (new_tstate == NULL) {
        return NULL;
    }
    /* We serialize concurrent creation to protect global state. */
    HEAD_LOCK(runtime);

    interp->threads.next_unique_id += 1;
    uint64_t id = interp->threads.next_unique_id;

    // Allocate the thread state and add it to the interpreter.
    PyThreadState *old_head = interp->threads.head;
    if (old_head == NULL) {
        // It's the interpreter's initial thread state.
        assert(id == 1);
        used_newtstate = 0;
        tstate = &interp->_initial_thread;
    }
    else {
        // Every valid interpreter must have at least one thread.
        assert(id > 1);
        assert(old_head->prev == NULL);
        used_newtstate = 1;
        tstate = new_tstate;
        // Set to _PyThreadState_INIT.
        memcpy(tstate,
               &initial._main_interpreter._initial_thread,
               sizeof(*tstate));
    }
    interp->threads.head = tstate;

    init_threadstate(tstate, interp, id, old_head);

    HEAD_UNLOCK(runtime);
    if (!used_newtstate) {
        // Must be called with lock unlocked to avoid re-entrancy deadlock.
        PyMem_RawFree(new_tstate);
    }
    return tstate;
}

PyThreadState *
PyThreadState_New(PyInterpreterState *interp)
{
    PyThreadState *tstate = new_threadstate(interp);
    if (tstate) {
        bind_tstate(tstate);
        // This makes sure there's a gilstate tstate bound
        // as soon as possible.
        if (gilstate_tss_get(tstate->interp->runtime) == NULL) {
            bind_gilstate_tstate(tstate);
        }
    }
    return tstate;
}

// This must be followed by a call to _PyThreadState_Bind();
PyThreadState *
_PyThreadState_New(PyInterpreterState *interp)
{
    return new_threadstate(interp);
}

// We keep this for stable ABI compabibility.
PyThreadState *
_PyThreadState_Prealloc(PyInterpreterState *interp)
{
    return _PyThreadState_New(interp);
}

// We keep this around for (accidental) stable ABI compatibility.
// Realistically, no extensions are using it.
void
_PyThreadState_Init(PyThreadState *tstate)
{
    Py_FatalError("_PyThreadState_Init() is for internal use only");
}

void
PyThreadState_Clear(PyThreadState *tstate)
{
    assert(tstate->_status.initialized && !tstate->_status.cleared);
    // XXX assert(!tstate->_status.bound || tstate->_status.unbound);
    tstate->_status.finalizing = 1;  // just in case

    /* XXX Conditions we need to enforce:

       * the GIL must be held by the current thread
       * current_fast_get()->interp must match tstate->interp
       * for the main interpreter, current_fast_get() must be the main thread
     */

    int verbose = _PyInterpreterState_GetConfig(tstate->interp)->verbose;

    if (verbose && tstate->cframe->current_frame != NULL) {
        /* bpo-20526: After the main thread calls
           _PyRuntimeState_SetFinalizing() in Py_FinalizeEx(), threads must
           exit when trying to take the GIL. If a thread exit in the middle of
           _PyEval_EvalFrameDefault(), tstate->frame is not reset to its
           previous value. It is more likely with daemon threads, but it can
           happen with regular threads if threading._shutdown() fails
           (ex: interrupted by CTRL+C). */
        fprintf(stderr,
          "PyThreadState_Clear: warning: thread still has a frame\n");
    }

    /* At this point tstate shouldn't be used any more,
       neither to run Python code nor for other uses.

       This is tricky when current_fast_get() == tstate, in the same way
       as noted in interpreter_clear() above.  The below finalizers
       can possibly run Python code or otherwise use the partially
       cleared thread state.  For now we trust that isn't a problem
       in practice.
     */
    // XXX Deal with the possibility of problematic finalizers.

    /* Don't clear tstate->pyframe: it is a borrowed reference */

    Py_CLEAR(tstate->dict);
    Py_CLEAR(tstate->async_exc);

    Py_CLEAR(tstate->current_exception);

    Py_CLEAR(tstate->exc_state.exc_value);

    /* The stack of exception states should contain just this thread. */
    if (verbose && tstate->exc_info != &tstate->exc_state) {
        fprintf(stderr,
          "PyThreadState_Clear: warning: thread still has a generator\n");
    }

    tstate->c_profilefunc = NULL;
    tstate->c_tracefunc = NULL;
    Py_CLEAR(tstate->c_profileobj);
    Py_CLEAR(tstate->c_traceobj);

    Py_CLEAR(tstate->async_gen_firstiter);
    Py_CLEAR(tstate->async_gen_finalizer);

    Py_CLEAR(tstate->context);

    if (tstate->on_delete != NULL) {
        tstate->on_delete(tstate->on_delete_data);
    }

    tstate->_status.cleared = 1;

    // XXX Call _PyThreadStateSwap(runtime, NULL) here if "current".
    // XXX Do it as early in the function as possible.
}


/* Common code for PyThreadState_Delete() and PyThreadState_DeleteCurrent() */
static void
tstate_delete_common(PyThreadState *tstate)
{
    assert(tstate->_status.cleared && !tstate->_status.finalized);

    PyInterpreterState *interp = tstate->interp;
    if (interp == NULL) {
        Py_FatalError("NULL interpreter");
    }
    _PyRuntimeState *runtime = interp->runtime;

    HEAD_LOCK(runtime);
    if (tstate->prev) {
        tstate->prev->next = tstate->next;
    }
    else {
        interp->threads.head = tstate->next;
    }
    if (tstate->next) {
        tstate->next->prev = tstate->prev;
    }
    HEAD_UNLOCK(runtime);

    // XXX Unbind in PyThreadState_Clear(), or earlier
    // (and assert not-equal here)?
    if (tstate->_status.bound_gilstate) {
        unbind_gilstate_tstate(tstate);
    }
    unbind_tstate(tstate);

    // XXX Move to PyThreadState_Clear()?
    _PyStackChunk *chunk = tstate->datastack_chunk;
    tstate->datastack_chunk = NULL;
    while (chunk != NULL) {
        _PyStackChunk *prev = chunk->previous;
        _PyObject_VirtualFree(chunk, chunk->size);
        chunk = prev;
    }

    tstate->_status.finalized = 1;
}


static void
zapthreads(PyInterpreterState *interp)
{
    PyThreadState *tstate;
    /* No need to lock the mutex here because this should only happen
       when the threads are all really dead (XXX famous last words). */
    while ((tstate = interp->threads.head) != NULL) {
        tstate_verify_not_active(tstate);
        tstate_delete_common(tstate);
        free_threadstate(tstate);
    }
}


void
PyThreadState_Delete(PyThreadState *tstate)
{
    _Py_EnsureTstateNotNULL(tstate);
    tstate_verify_not_active(tstate);
    tstate_delete_common(tstate);
    free_threadstate(tstate);
}


void
_PyThreadState_DeleteCurrent(PyThreadState *tstate)
{
    _Py_EnsureTstateNotNULL(tstate);
    tstate_delete_common(tstate);
    current_fast_clear(tstate->interp->runtime);
    _PyEval_ReleaseLock(tstate);
    free_threadstate(tstate);
}

void
PyThreadState_DeleteCurrent(void)
{
    PyThreadState *tstate = current_fast_get(&_PyRuntime);
    _PyThreadState_DeleteCurrent(tstate);
}


/*
 * Delete all thread states except the one passed as argument.
 * Note that, if there is a current thread state, it *must* be the one
 * passed as argument.  Also, this won't touch any other interpreters
 * than the current one, since we don't know which thread state should
 * be kept in those other interpreters.
 */
void
_PyThreadState_DeleteExcept(PyThreadState *tstate)
{
    assert(tstate != NULL);
    PyInterpreterState *interp = tstate->interp;
    _PyRuntimeState *runtime = interp->runtime;

    HEAD_LOCK(runtime);
    /* Remove all thread states, except tstate, from the linked list of
       thread states.  This will allow calling PyThreadState_Clear()
       without holding the lock. */
    PyThreadState *list = interp->threads.head;
    if (list == tstate) {
        list = tstate->next;
    }
    if (tstate->prev) {
        tstate->prev->next = tstate->next;
    }
    if (tstate->next) {
        tstate->next->prev = tstate->prev;
    }
    tstate->prev = tstate->next = NULL;
    interp->threads.head = tstate;
    HEAD_UNLOCK(runtime);

    /* Clear and deallocate all stale thread states.  Even if this
       executes Python code, we should be safe since it executes
       in the current thread, not one of the stale threads. */
    PyThreadState *p, *next;
    for (p = list; p; p = next) {
        next = p->next;
        PyThreadState_Clear(p);
        free_threadstate(p);
    }
}


//----------
// accessors
//----------

/* An extension mechanism to store arbitrary additional per-thread state.
   PyThreadState_GetDict() returns a dictionary that can be used to hold such
   state; the caller should pick a unique key and store its state there.  If
   PyThreadState_GetDict() returns NULL, an exception has *not* been raised
   and the caller should assume no per-thread state is available. */

PyObject *
_PyThreadState_GetDict(PyThreadState *tstate)
{
    assert(tstate != NULL);
    if (tstate->dict == NULL) {
        tstate->dict = PyDict_New();
        if (tstate->dict == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    return tstate->dict;
}


PyObject *
PyThreadState_GetDict(void)
{
    PyThreadState *tstate = current_fast_get(&_PyRuntime);
    if (tstate == NULL) {
        return NULL;
    }
    return _PyThreadState_GetDict(tstate);
}


PyInterpreterState *
PyThreadState_GetInterpreter(PyThreadState *tstate)
{
    assert(tstate != NULL);
    return tstate->interp;
}


PyFrameObject*
PyThreadState_GetFrame(PyThreadState *tstate)
{
    assert(tstate != NULL);
    _PyInterpreterFrame *f = _PyThreadState_GetFrame(tstate);
    if (f == NULL) {
        return NULL;
    }
    PyFrameObject *frame = _PyFrame_GetFrameObject(f);
    if (frame == NULL) {
        PyErr_Clear();
    }
    return (PyFrameObject*)Py_XNewRef(frame);
}


uint64_t
PyThreadState_GetID(PyThreadState *tstate)
{
    assert(tstate != NULL);
    return tstate->id;
}


static inline void
tstate_activate(PyThreadState *tstate)
{
    assert(tstate != NULL);
    // XXX assert(tstate_is_alive(tstate));
    assert(tstate_is_bound(tstate));
    assert(!tstate->_status.active);

    assert(!tstate->_status.bound_gilstate ||
           tstate == gilstate_tss_get((tstate->interp->runtime)));
    if (!tstate->_status.bound_gilstate) {
        bind_gilstate_tstate(tstate);
    }

    tstate->_status.active = 1;
}

static inline void
tstate_deactivate(PyThreadState *tstate)
{
    assert(tstate != NULL);
    // XXX assert(tstate_is_alive(tstate));
    assert(tstate_is_bound(tstate));
    assert(tstate->_status.active);

    tstate->_status.active = 0;

    // We do not unbind the gilstate tstate here.
    // It will still be used in PyGILState_Ensure().
}


//----------
// other API
//----------

/* Asynchronously raise an exception in a thread.
   Requested by Just van Rossum and Alex Martelli.
   To prevent naive misuse, you must write your own extension
   to call this, or use ctypes.  Must be called with the GIL held.
   Returns the number of tstates modified (normally 1, but 0 if `id` didn't
   match any known thread id).  Can be called with exc=NULL to clear an
   existing async exception.  This raises no exceptions. */

// XXX Move this to Python/ceval_gil.c?
// XXX Deprecate this.
int
PyThreadState_SetAsyncExc(unsigned long id, PyObject *exc)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    PyInterpreterState *interp = _PyRuntimeState_GetThreadState(runtime)->interp;

    /* Although the GIL is held, a few C API functions can be called
     * without the GIL held, and in particular some that create and
     * destroy thread and interpreter states.  Those can mutate the
     * list of thread states we're traversing, so to prevent that we lock
     * head_mutex for the duration.
     */
    HEAD_LOCK(runtime);
    for (PyThreadState *tstate = interp->threads.head; tstate != NULL; tstate = tstate->next) {
        if (tstate->thread_id != id) {
            continue;
        }

        /* Tricky:  we need to decref the current value
         * (if any) in tstate->async_exc, but that can in turn
         * allow arbitrary Python code to run, including
         * perhaps calls to this function.  To prevent
         * deadlock, we need to release head_mutex before
         * the decref.
         */
        PyObject *old_exc = tstate->async_exc;
        tstate->async_exc = Py_XNewRef(exc);
        HEAD_UNLOCK(runtime);

        Py_XDECREF(old_exc);
        _PyEval_SignalAsyncExc(tstate->interp);
        return 1;
    }
    HEAD_UNLOCK(runtime);
    return 0;
}


//---------------------------------
// API for the current thread state
//---------------------------------

PyThreadState *
_PyThreadState_UncheckedGet(void)
{
    return current_fast_get(&_PyRuntime);
}


PyThreadState *
PyThreadState_Get(void)
{
    PyThreadState *tstate = current_fast_get(&_PyRuntime);
    _Py_EnsureTstateNotNULL(tstate);
    return tstate;
}


PyThreadState *
_PyThreadState_Swap(_PyRuntimeState *runtime, PyThreadState *newts)
{
#if defined(Py_DEBUG)
    /* This can be called from PyEval_RestoreThread(). Similar
       to it, we need to ensure errno doesn't change.
    */
    int err = errno;
#endif
    PyThreadState *oldts = current_fast_get(runtime);

    current_fast_clear(runtime);

    if (oldts != NULL) {
        // XXX assert(tstate_is_alive(oldts) && tstate_is_bound(oldts));
        tstate_deactivate(oldts);
    }

    if (newts != NULL) {
        // XXX assert(tstate_is_alive(newts));
        assert(tstate_is_bound(newts));
        current_fast_set(runtime, newts);
        tstate_activate(newts);
    }

#if defined(Py_DEBUG)
    errno = err;
#endif
    return oldts;
}

PyThreadState *
PyThreadState_Swap(PyThreadState *newts)
{
    return _PyThreadState_Swap(&_PyRuntime, newts);
}


void
_PyThreadState_Bind(PyThreadState *tstate)
{
    bind_tstate(tstate);
    // This makes sure there's a gilstate tstate bound
    // as soon as possible.
    if (gilstate_tss_get(tstate->interp->runtime) == NULL) {
        bind_gilstate_tstate(tstate);
    }
}


/***********************************/
/* routines for advanced debuggers */
/***********************************/

// (requested by David Beazley)
// Don't use unless you know what you are doing!

PyInterpreterState *
PyInterpreterState_Head(void)
{
    return _PyRuntime.interpreters.head;
}

PyInterpreterState *
PyInterpreterState_Main(void)
{
    return _PyInterpreterState_Main();
}

PyInterpreterState *
PyInterpreterState_Next(PyInterpreterState *interp) {
    return interp->next;
}

PyThreadState *
PyInterpreterState_ThreadHead(PyInterpreterState *interp) {
    return interp->threads.head;
}

PyThreadState *
PyThreadState_Next(PyThreadState *tstate) {
    return tstate->next;
}


/********************************************/
/* reporting execution state of all threads */
/********************************************/

/* The implementation of sys._current_frames().  This is intended to be
   called with the GIL held, as it will be when called via
   sys._current_frames().  It's possible it would work fine even without
   the GIL held, but haven't thought enough about that.
*/
PyObject *
_PyThread_CurrentFrames(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    PyThreadState *tstate = current_fast_get(runtime);
    if (_PySys_Audit(tstate, "sys._current_frames", NULL) < 0) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (result == NULL) {
        return NULL;
    }

    /* for i in all interpreters:
     *     for t in all of i's thread states:
     *          if t's frame isn't NULL, map t's id to its frame
     * Because these lists can mutate even when the GIL is held, we
     * need to grab head_mutex for the duration.
     */
    HEAD_LOCK(runtime);
    PyInterpreterState *i;
    for (i = runtime->interpreters.head; i != NULL; i = i->next) {
        PyThreadState *t;
        for (t = i->threads.head; t != NULL; t = t->next) {
            _PyInterpreterFrame *frame = t->cframe->current_frame;
            frame = _PyFrame_GetFirstComplete(frame);
            if (frame == NULL) {
                continue;
            }
            PyObject *id = PyLong_FromUnsignedLong(t->thread_id);
            if (id == NULL) {
                goto fail;
            }
            PyObject *frameobj = (PyObject *)_PyFrame_GetFrameObject(frame);
            if (frameobj == NULL) {
                Py_DECREF(id);
                goto fail;
            }
            int stat = PyDict_SetItem(result, id, frameobj);
            Py_DECREF(id);
            if (stat < 0) {
                goto fail;
            }
        }
    }
    goto done;

fail:
    Py_CLEAR(result);

done:
    HEAD_UNLOCK(runtime);
    return result;
}

/* The implementation of sys._current_exceptions().  This is intended to be
   called with the GIL held, as it will be when called via
   sys._current_exceptions().  It's possible it would work fine even without
   the GIL held, but haven't thought enough about that.
*/
PyObject *
_PyThread_CurrentExceptions(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    PyThreadState *tstate = current_fast_get(runtime);

    _Py_EnsureTstateNotNULL(tstate);

    if (_PySys_Audit(tstate, "sys._current_exceptions", NULL) < 0) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (result == NULL) {
        return NULL;
    }

    /* for i in all interpreters:
     *     for t in all of i's thread states:
     *          if t's frame isn't NULL, map t's id to its frame
     * Because these lists can mutate even when the GIL is held, we
     * need to grab head_mutex for the duration.
     */
    HEAD_LOCK(runtime);
    PyInterpreterState *i;
    for (i = runtime->interpreters.head; i != NULL; i = i->next) {
        PyThreadState *t;
        for (t = i->threads.head; t != NULL; t = t->next) {
            _PyErr_StackItem *err_info = _PyErr_GetTopmostException(t);
            if (err_info == NULL) {
                continue;
            }
            PyObject *id = PyLong_FromUnsignedLong(t->thread_id);
            if (id == NULL) {
                goto fail;
            }
            PyObject *exc_info = _PyErr_StackItemToExcInfoTuple(err_info);
            if (exc_info == NULL) {
                Py_DECREF(id);
                goto fail;
            }
            int stat = PyDict_SetItem(result, id, exc_info);
            Py_DECREF(id);
            Py_DECREF(exc_info);
            if (stat < 0) {
                goto fail;
            }
        }
    }
    goto done;

fail:
    Py_CLEAR(result);

done:
    HEAD_UNLOCK(runtime);
    return result;
}


/****************/
/* module state */
/****************/

PyObject*
PyState_FindModule(PyModuleDef* module)
{
    Py_ssize_t index = module->m_base.m_index;
    PyInterpreterState *state = _PyInterpreterState_GET();
    PyObject *res;
    if (module->m_slots) {
        return NULL;
    }
    if (index == 0)
        return NULL;
    if (state->modules_by_index == NULL)
        return NULL;
    if (index >= PyList_GET_SIZE(state->modules_by_index))
        return NULL;
    res = PyList_GET_ITEM(state->modules_by_index, index);
    return res==Py_None ? NULL : res;
}

int
_PyState_AddModule(PyThreadState *tstate, PyObject* module, PyModuleDef* def)
{
    if (!def) {
        assert(_PyErr_Occurred(tstate));
        return -1;
    }
    if (def->m_slots) {
        _PyErr_SetString(tstate,
                         PyExc_SystemError,
                         "PyState_AddModule called on module with slots");
        return -1;
    }

    PyInterpreterState *interp = tstate->interp;
    if (!interp->modules_by_index) {
        interp->modules_by_index = PyList_New(0);
        if (!interp->modules_by_index) {
            return -1;
        }
    }

    while (PyList_GET_SIZE(interp->modules_by_index) <= def->m_base.m_index) {
        if (PyList_Append(interp->modules_by_index, Py_None) < 0) {
            return -1;
        }
    }

    return PyList_SetItem(interp->modules_by_index,
                          def->m_base.m_index, Py_NewRef(module));
}

int
PyState_AddModule(PyObject* module, PyModuleDef* def)
{
    if (!def) {
        Py_FatalError("module definition is NULL");
        return -1;
    }

    PyThreadState *tstate = current_fast_get(&_PyRuntime);
    PyInterpreterState *interp = tstate->interp;
    Py_ssize_t index = def->m_base.m_index;
    if (interp->modules_by_index &&
        index < PyList_GET_SIZE(interp->modules_by_index) &&
        module == PyList_GET_ITEM(interp->modules_by_index, index))
    {
        _Py_FatalErrorFormat(__func__, "module %p already added", module);
        return -1;
    }
    return _PyState_AddModule(tstate, module, def);
}

int
PyState_RemoveModule(PyModuleDef* def)
{
    PyThreadState *tstate = current_fast_get(&_PyRuntime);
    PyInterpreterState *interp = tstate->interp;

    if (def->m_slots) {
        _PyErr_SetString(tstate,
                         PyExc_SystemError,
                         "PyState_RemoveModule called on module with slots");
        return -1;
    }

    Py_ssize_t index = def->m_base.m_index;
    if (index == 0) {
        Py_FatalError("invalid module index");
    }
    if (interp->modules_by_index == NULL) {
        Py_FatalError("Interpreters module-list not accessible.");
    }
    if (index > PyList_GET_SIZE(interp->modules_by_index)) {
        Py_FatalError("Module index out of bounds.");
    }

    return PyList_SetItem(interp->modules_by_index, index, Py_NewRef(Py_None));
}


/***********************************/
/* Python "auto thread state" API. */
/***********************************/

/* Internal initialization/finalization functions called by
   Py_Initialize/Py_FinalizeEx
*/
PyStatus
_PyGILState_Init(PyInterpreterState *interp)
{
    if (!_Py_IsMainInterpreter(interp)) {
        /* Currently, PyGILState is shared by all interpreters. The main
         * interpreter is responsible to initialize it. */
        return _PyStatus_OK();
    }
    _PyRuntimeState *runtime = interp->runtime;
    assert(gilstate_tss_get(runtime) == NULL);
    assert(runtime->gilstate.autoInterpreterState == NULL);
    runtime->gilstate.autoInterpreterState = interp;
    return _PyStatus_OK();
}

void
_PyGILState_Fini(PyInterpreterState *interp)
{
    if (!_Py_IsMainInterpreter(interp)) {
        /* Currently, PyGILState is shared by all interpreters. The main
         * interpreter is responsible to initialize it. */
        return;
    }
    interp->runtime->gilstate.autoInterpreterState = NULL;
}


// XXX Drop this.
PyStatus
_PyGILState_SetTstate(PyThreadState *tstate)
{
    /* must init with valid states */
    assert(tstate != NULL);
    assert(tstate->interp != NULL);

    if (!_Py_IsMainInterpreter(tstate->interp)) {
        /* Currently, PyGILState is shared by all interpreters. The main
         * interpreter is responsible to initialize it. */
        return _PyStatus_OK();
    }

#ifndef NDEBUG
    _PyRuntimeState *runtime = tstate->interp->runtime;

    assert(runtime->gilstate.autoInterpreterState == tstate->interp);
    assert(gilstate_tss_get(runtime) == tstate);
    assert(tstate->gilstate_counter == 1);
#endif

    return _PyStatus_OK();
}

PyInterpreterState *
_PyGILState_GetInterpreterStateUnsafe(void)
{
    return _PyRuntime.gilstate.autoInterpreterState;
}

/* The public functions */

PyThreadState *
PyGILState_GetThisThreadState(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    if (!gilstate_tss_initialized(runtime)) {
        return NULL;
    }
    return gilstate_tss_get(runtime);
}

int
PyGILState_Check(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    if (!runtime->gilstate.check_enabled) {
        return 1;
    }

    if (!gilstate_tss_initialized(runtime)) {
        return 1;
    }

    PyThreadState *tstate = current_fast_get(runtime);
    if (tstate == NULL) {
        return 0;
    }

    return (tstate == gilstate_tss_get(runtime));
}

PyGILState_STATE
PyGILState_Ensure(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;

    /* Note that we do not auto-init Python here - apart from
       potential races with 2 threads auto-initializing, pep-311
       spells out other issues.  Embedders are expected to have
       called Py_Initialize(). */

    /* Ensure that _PyEval_InitThreads() and _PyGILState_Init() have been
       called by Py_Initialize() */
    assert(_PyEval_ThreadsInitialized(runtime));
    assert(gilstate_tss_initialized(runtime));
    assert(runtime->gilstate.autoInterpreterState != NULL);

    PyThreadState *tcur = gilstate_tss_get(runtime);
    int has_gil;
    if (tcur == NULL) {
        /* Create a new Python thread state for this thread */
        tcur = new_threadstate(runtime->gilstate.autoInterpreterState);
        if (tcur == NULL) {
            Py_FatalError("Couldn't create thread-state for new thread");
        }
        bind_tstate(tcur);
        bind_gilstate_tstate(tcur);

        /* This is our thread state!  We'll need to delete it in the
           matching call to PyGILState_Release(). */
        assert(tcur->gilstate_counter == 1);
        tcur->gilstate_counter = 0;
        has_gil = 0; /* new thread state is never current */
    }
    else {
        has_gil = holds_gil(tcur);
    }

    if (!has_gil) {
        PyEval_RestoreThread(tcur);
    }

    /* Update our counter in the thread-state - no need for locks:
       - tcur will remain valid as we hold the GIL.
       - the counter is safe as we are the only thread "allowed"
         to modify this value
    */
    ++tcur->gilstate_counter;

    return has_gil ? PyGILState_LOCKED : PyGILState_UNLOCKED;
}

void
PyGILState_Release(PyGILState_STATE oldstate)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    PyThreadState *tstate = gilstate_tss_get(runtime);
    if (tstate == NULL) {
        Py_FatalError("auto-releasing thread-state, "
                      "but no thread-state for this thread");
    }

    /* We must hold the GIL and have our thread state current */
    /* XXX - remove the check - the assert should be fine,
       but while this is very new (April 2003), the extra check
       by release-only users can't hurt.
    */
    if (!holds_gil(tstate)) {
        _Py_FatalErrorFormat(__func__,
                             "thread state %p must be current when releasing",
                             tstate);
    }
    assert(holds_gil(tstate));
    --tstate->gilstate_counter;
    assert(tstate->gilstate_counter >= 0); /* illegal counter value */

    /* If we're going to destroy this thread-state, we must
     * clear it while the GIL is held, as destructors may run.
     */
    if (tstate->gilstate_counter == 0) {
        /* can't have been locked when we created it */
        assert(oldstate == PyGILState_UNLOCKED);
        // XXX Unbind tstate here.
        PyThreadState_Clear(tstate);
        /* Delete the thread-state.  Note this releases the GIL too!
         * It's vital that the GIL be held here, to avoid shutdown
         * races; see bugs 225673 and 1061968 (that nasty bug has a
         * habit of coming back).
         */
        assert(current_fast_get(runtime) == tstate);
        _PyThreadState_DeleteCurrent(tstate);
    }
    /* Release the lock if necessary */
    else if (oldstate == PyGILState_UNLOCKED) {
        PyEval_SaveThread();
    }
}


/**************************/
/* cross-interpreter data */
/**************************/

/* cross-interpreter data */

static inline void
_xidata_init(_PyCrossInterpreterData *data)
{
    // If the value is being reused
    // then _xidata_clear() should have been called already.
    assert(data->data == NULL);
    assert(data->obj == NULL);
    *data = (_PyCrossInterpreterData){0};
    data->interp = -1;
}

static inline void
_xidata_clear(_PyCrossInterpreterData *data)
{
    if (data->free != NULL) {
        data->free(data->data);
    }
    data->data = NULL;
    Py_CLEAR(data->obj);
}

void
_PyCrossInterpreterData_Init(_PyCrossInterpreterData *data,
                             PyInterpreterState *interp,
                             void *shared, PyObject *obj,
                             xid_newobjectfunc new_object)
{
    assert(data != NULL);
    assert(new_object != NULL);
    _xidata_init(data);
    data->data = shared;
    if (obj != NULL) {
        assert(interp != NULL);
        // released in _PyCrossInterpreterData_Clear()
        data->obj = Py_NewRef(obj);
    }
    // Ideally every object would know its owning interpreter.
    // Until then, we have to rely on the caller to identify it
    // (but we don't need it in all cases).
    data->interp = (interp != NULL) ? interp->id : -1;
    data->new_object = new_object;
}

int
_PyCrossInterpreterData_InitWithSize(_PyCrossInterpreterData *data,
                                     PyInterpreterState *interp,
                                     const size_t size, PyObject *obj,
                                     xid_newobjectfunc new_object)
{
    assert(size > 0);
    // For now we always free the shared data in the same interpreter
    // where it was allocated, so the interpreter is required.
    assert(interp != NULL);
    _PyCrossInterpreterData_Init(data, interp, NULL, obj, new_object);
    data->data = PyMem_Malloc(size);
    if (data->data == NULL) {
        return -1;
    }
    data->free = PyMem_Free;
    return 0;
}

void
_PyCrossInterpreterData_Clear(PyInterpreterState *interp,
                              _PyCrossInterpreterData *data)
{
    assert(data != NULL);
    // This must be called in the owning interpreter.
    assert(interp == NULL || data->interp == interp->id);
    _xidata_clear(data);
}

static int
_check_xidata(PyThreadState *tstate, _PyCrossInterpreterData *data)
{
    // data->data can be anything, including NULL, so we don't check it.

    // data->obj may be NULL, so we don't check it.

    if (data->interp < 0) {
        _PyErr_SetString(tstate, PyExc_SystemError, "missing interp");
        return -1;
    }

    if (data->new_object == NULL) {
        _PyErr_SetString(tstate, PyExc_SystemError, "missing new_object func");
        return -1;
    }

    // data->free may be NULL, so we don't check it.

    return 0;
}

crossinterpdatafunc _PyCrossInterpreterData_Lookup(PyObject *);

/* This is a separate func from _PyCrossInterpreterData_Lookup in order
   to keep the registry code separate. */
static crossinterpdatafunc
_lookup_getdata(PyObject *obj)
{
    crossinterpdatafunc getdata = _PyCrossInterpreterData_Lookup(obj);
    if (getdata == NULL && PyErr_Occurred() == 0)
        PyErr_Format(PyExc_ValueError,
                     "%S does not support cross-interpreter data", obj);
    return getdata;
}

int
_PyObject_CheckCrossInterpreterData(PyObject *obj)
{
    crossinterpdatafunc getdata = _lookup_getdata(obj);
    if (getdata == NULL) {
        return -1;
    }
    return 0;
}

int
_PyObject_GetCrossInterpreterData(PyObject *obj, _PyCrossInterpreterData *data)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    PyThreadState *tstate = current_fast_get(runtime);
#ifdef Py_DEBUG
    // The caller must hold the GIL
    _Py_EnsureTstateNotNULL(tstate);
#endif
    PyInterpreterState *interp = tstate->interp;

    // Reset data before re-populating.
    *data = (_PyCrossInterpreterData){0};
    data->interp = -1;

    // Call the "getdata" func for the object.
    Py_INCREF(obj);
    crossinterpdatafunc getdata = _lookup_getdata(obj);
    if (getdata == NULL) {
        Py_DECREF(obj);
        return -1;
    }
    int res = getdata(tstate, obj, data);
    Py_DECREF(obj);
    if (res != 0) {
        return -1;
    }

    // Fill in the blanks and validate the result.
    data->interp = interp->id;
    if (_check_xidata(tstate, data) != 0) {
        (void)_PyCrossInterpreterData_Release(data);
        return -1;
    }

    return 0;
}

PyObject *
_PyCrossInterpreterData_NewObject(_PyCrossInterpreterData *data)
{
    return data->new_object(data);
}

typedef void (*releasefunc)(PyInterpreterState *, void *);

static void
_call_in_interpreter(PyInterpreterState *interp, releasefunc func, void *arg)
{
    /* We would use Py_AddPendingCall() if it weren't specific to the
     * main interpreter (see bpo-33608).  In the meantime we take a
     * naive approach.
     */
    _PyRuntimeState *runtime = interp->runtime;
    PyThreadState *save_tstate = NULL;
    if (interp != current_fast_get(runtime)->interp) {
        // XXX Using the "head" thread isn't strictly correct.
        PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);
        // XXX Possible GILState issues?
        save_tstate = _PyThreadState_Swap(runtime, tstate);
    }

    // XXX Once the GIL is per-interpreter, this should be called with the
    // calling interpreter's GIL released and the target interpreter's held.
    func(interp, arg);

    // Switch back.
    if (save_tstate != NULL) {
        _PyThreadState_Swap(runtime, save_tstate);
    }
}

int
_PyCrossInterpreterData_Release(_PyCrossInterpreterData *data)
{
    if (data->free == NULL && data->obj == NULL) {
        // Nothing to release!
        data->data = NULL;
        return 0;
    }

    // Switch to the original interpreter.
    PyInterpreterState *interp = _PyInterpreterState_LookUpID(data->interp);
    if (interp == NULL) {
        // The interpreter was already destroyed.
        // This function shouldn't have been called.
        // XXX Someone leaked some memory...
        assert(PyErr_Occurred());
        return -1;
    }

    // "Release" the data and/or the object.
    _call_in_interpreter(interp,
                         (releasefunc)_PyCrossInterpreterData_Clear, data);
    return 0;
}

/* registry of {type -> crossinterpdatafunc} */

/* For now we use a global registry of shareable classes.  An
   alternative would be to add a tp_* slot for a class's
   crossinterpdatafunc. It would be simpler and more efficient. */

static int
_xidregistry_add_type(struct _xidregistry *xidregistry, PyTypeObject *cls,
                 crossinterpdatafunc getdata)
{
    // Note that we effectively replace already registered classes
    // rather than failing.
    struct _xidregitem *newhead = PyMem_RawMalloc(sizeof(struct _xidregitem));
    if (newhead == NULL) {
        return -1;
    }
    // XXX Assign a callback to clear the entry from the registry?
    newhead->cls = PyWeakref_NewRef((PyObject *)cls, NULL);
    if (newhead->cls == NULL) {
        PyMem_RawFree(newhead);
        return -1;
    }
    newhead->getdata = getdata;
    newhead->prev = NULL;
    newhead->next = xidregistry->head;
    if (newhead->next != NULL) {
        newhead->next->prev = newhead;
    }
    xidregistry->head = newhead;
    return 0;
}

static struct _xidregitem *
_xidregistry_remove_entry(struct _xidregistry *xidregistry,
                          struct _xidregitem *entry)
{
    struct _xidregitem *next = entry->next;
    if (entry->prev != NULL) {
        assert(entry->prev->next == entry);
        entry->prev->next = next;
    }
    else {
        assert(xidregistry->head == entry);
        xidregistry->head = next;
    }
    if (next != NULL) {
        next->prev = entry->prev;
    }
    Py_DECREF(entry->cls);
    PyMem_RawFree(entry);
    return next;
}

static struct _xidregitem *
_xidregistry_find_type(struct _xidregistry *xidregistry, PyTypeObject *cls)
{
    struct _xidregitem *cur = xidregistry->head;
    while (cur != NULL) {
        PyObject *registered = PyWeakref_GetObject(cur->cls);
        if (registered == Py_None) {
            // The weakly ref'ed object was freed.
            cur = _xidregistry_remove_entry(xidregistry, cur);
        }
        else {
            assert(PyType_Check(registered));
            if (registered == (PyObject *)cls) {
                return cur;
            }
            cur = cur->next;
        }
    }
    return NULL;
}

static void _register_builtins_for_crossinterpreter_data(struct _xidregistry *xidregistry);

int
_PyCrossInterpreterData_RegisterClass(PyTypeObject *cls,
                                       crossinterpdatafunc getdata)
{
    if (!PyType_Check(cls)) {
        PyErr_Format(PyExc_ValueError, "only classes may be registered");
        return -1;
    }
    if (getdata == NULL) {
        PyErr_Format(PyExc_ValueError, "missing 'getdata' func");
        return -1;
    }

    struct _xidregistry *xidregistry = &_PyRuntime.xidregistry ;
    PyThread_acquire_lock(xidregistry->mutex, WAIT_LOCK);
    if (xidregistry->head == NULL) {
        _register_builtins_for_crossinterpreter_data(xidregistry);
    }
    int res = _xidregistry_add_type(xidregistry, cls, getdata);
    PyThread_release_lock(xidregistry->mutex);
    return res;
}

int
_PyCrossInterpreterData_UnregisterClass(PyTypeObject *cls)
{
    int res = 0;
    struct _xidregistry *xidregistry = &_PyRuntime.xidregistry ;
    PyThread_acquire_lock(xidregistry->mutex, WAIT_LOCK);
    struct _xidregitem *matched = _xidregistry_find_type(xidregistry, cls);
    if (matched != NULL) {
        (void)_xidregistry_remove_entry(xidregistry, matched);
        res = 1;
    }
    PyThread_release_lock(xidregistry->mutex);
    return res;
}


/* Cross-interpreter objects are looked up by exact match on the class.
   We can reassess this policy when we move from a global registry to a
   tp_* slot. */

crossinterpdatafunc
_PyCrossInterpreterData_Lookup(PyObject *obj)
{
    struct _xidregistry *xidregistry = &_PyRuntime.xidregistry ;
    PyObject *cls = PyObject_Type(obj);
    PyThread_acquire_lock(xidregistry->mutex, WAIT_LOCK);
    if (xidregistry->head == NULL) {
        _register_builtins_for_crossinterpreter_data(xidregistry);
    }
    struct _xidregitem *matched = _xidregistry_find_type(xidregistry,
                                                         (PyTypeObject *)cls);
    Py_DECREF(cls);
    PyThread_release_lock(xidregistry->mutex);
    return matched != NULL ? matched->getdata : NULL;
}

/* cross-interpreter data for builtin types */

struct _shared_bytes_data {
    char *bytes;
    Py_ssize_t len;
};

static PyObject *
_new_bytes_object(_PyCrossInterpreterData *data)
{
    struct _shared_bytes_data *shared = (struct _shared_bytes_data *)(data->data);
    return PyBytes_FromStringAndSize(shared->bytes, shared->len);
}

static int
_bytes_shared(PyThreadState *tstate, PyObject *obj,
              _PyCrossInterpreterData *data)
{
    if (_PyCrossInterpreterData_InitWithSize(
            data, tstate->interp, sizeof(struct _shared_bytes_data), obj,
            _new_bytes_object
            ) < 0)
    {
        return -1;
    }
    struct _shared_bytes_data *shared = (struct _shared_bytes_data *)data->data;
    if (PyBytes_AsStringAndSize(obj, &shared->bytes, &shared->len) < 0) {
        _PyCrossInterpreterData_Clear(tstate->interp, data);
        return -1;
    }
    return 0;
}

struct _shared_str_data {
    int kind;
    const void *buffer;
    Py_ssize_t len;
};

static PyObject *
_new_str_object(_PyCrossInterpreterData *data)
{
    struct _shared_str_data *shared = (struct _shared_str_data *)(data->data);
    return PyUnicode_FromKindAndData(shared->kind, shared->buffer, shared->len);
}

static int
_str_shared(PyThreadState *tstate, PyObject *obj,
            _PyCrossInterpreterData *data)
{
    if (_PyCrossInterpreterData_InitWithSize(
            data, tstate->interp, sizeof(struct _shared_str_data), obj,
            _new_str_object
            ) < 0)
    {
        return -1;
    }
    struct _shared_str_data *shared = (struct _shared_str_data *)data->data;
    shared->kind = PyUnicode_KIND(obj);
    shared->buffer = PyUnicode_DATA(obj);
    shared->len = PyUnicode_GET_LENGTH(obj);
    return 0;
}

static PyObject *
_new_long_object(_PyCrossInterpreterData *data)
{
    return PyLong_FromSsize_t((Py_ssize_t)(data->data));
}

static int
_long_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
{
    /* Note that this means the size of shareable ints is bounded by
     * sys.maxsize.  Hence on 32-bit architectures that is half the
     * size of maximum shareable ints on 64-bit.
     */
    Py_ssize_t value = PyLong_AsSsize_t(obj);
    if (value == -1 && PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_OverflowError)) {
            PyErr_SetString(PyExc_OverflowError, "try sending as bytes");
        }
        return -1;
    }
    _PyCrossInterpreterData_Init(data, tstate->interp, (void *)value, NULL,
            _new_long_object);
    // data->obj and data->free remain NULL
    return 0;
}

static PyObject *
_new_none_object(_PyCrossInterpreterData *data)
{
    // XXX Singleton refcounts are problematic across interpreters...
    return Py_NewRef(Py_None);
}

static int
_none_shared(PyThreadState *tstate, PyObject *obj,
             _PyCrossInterpreterData *data)
{
    _PyCrossInterpreterData_Init(data, tstate->interp, NULL, NULL,
            _new_none_object);
    // data->data, data->obj and data->free remain NULL
    return 0;
}

static void
_register_builtins_for_crossinterpreter_data(struct _xidregistry *xidregistry)
{
    // None
    if (_xidregistry_add_type(xidregistry, (PyTypeObject *)PyObject_Type(Py_None), _none_shared) != 0) {
        Py_FatalError("could not register None for cross-interpreter sharing");
    }

    // int
    if (_xidregistry_add_type(xidregistry, &PyLong_Type, _long_shared) != 0) {
        Py_FatalError("could not register int for cross-interpreter sharing");
    }

    // bytes
    if (_xidregistry_add_type(xidregistry, &PyBytes_Type, _bytes_shared) != 0) {
        Py_FatalError("could not register bytes for cross-interpreter sharing");
    }

    // str
    if (_xidregistry_add_type(xidregistry, &PyUnicode_Type, _str_shared) != 0) {
        Py_FatalError("could not register str for cross-interpreter sharing");
    }
}


_PyFrameEvalFunction
_PyInterpreterState_GetEvalFrameFunc(PyInterpreterState *interp)
{
    if (interp->eval_frame == NULL) {
        return _PyEval_EvalFrameDefault;
    }
    return interp->eval_frame;
}


void
_PyInterpreterState_SetEvalFrameFunc(PyInterpreterState *interp,
                                     _PyFrameEvalFunction eval_frame)
{
    if (eval_frame == _PyEval_EvalFrameDefault) {
        interp->eval_frame = NULL;
    }
    else {
        interp->eval_frame = eval_frame;
    }
}


const PyConfig*
_PyInterpreterState_GetConfig(PyInterpreterState *interp)
{
    return &interp->config;
}


int
_PyInterpreterState_GetConfigCopy(PyConfig *config)
{
    PyInterpreterState *interp = PyInterpreterState_Get();

    PyStatus status = _PyConfig_Copy(config, &interp->config);
    if (PyStatus_Exception(status)) {
        _PyErr_SetFromPyStatus(status);
        return -1;
    }
    return 0;
}


const PyConfig*
_Py_GetConfig(void)
{
    _PyRuntimeState *runtime = &_PyRuntime;
    assert(PyGILState_Check());
    PyThreadState *tstate = current_fast_get(runtime);
    _Py_EnsureTstateNotNULL(tstate);
    return _PyInterpreterState_GetConfig(tstate->interp);
}


int
_PyInterpreterState_HasFeature(PyInterpreterState *interp, unsigned long feature)
{
    return ((interp->feature_flags & feature) != 0);
}


#define MINIMUM_OVERHEAD 1000

static PyObject **
push_chunk(PyThreadState *tstate, int size)
{
    int allocate_size = DATA_STACK_CHUNK_SIZE;
    while (allocate_size < (int)sizeof(PyObject*)*(size + MINIMUM_OVERHEAD)) {
        allocate_size *= 2;
    }
    _PyStackChunk *new = allocate_chunk(allocate_size, tstate->datastack_chunk);
    if (new == NULL) {
        return NULL;
    }
    if (tstate->datastack_chunk) {
        tstate->datastack_chunk->top = tstate->datastack_top -
                                       &tstate->datastack_chunk->data[0];
    }
    tstate->datastack_chunk = new;
    tstate->datastack_limit = (PyObject **)(((char *)new) + allocate_size);
    // When new is the "root" chunk (i.e. new->previous == NULL), we can keep
    // _PyThreadState_PopFrame from freeing it later by "skipping" over the
    // first element:
    PyObject **res = &new->data[new->previous == NULL];
    tstate->datastack_top = res + size;
    return res;
}

_PyInterpreterFrame *
_PyThreadState_PushFrame(PyThreadState *tstate, size_t size)
{
    assert(size < INT_MAX/sizeof(PyObject *));
    if (_PyThreadState_HasStackSpace(tstate, (int)size)) {
        _PyInterpreterFrame *res = (_PyInterpreterFrame *)tstate->datastack_top;
        tstate->datastack_top += size;
        return res;
    }
    return (_PyInterpreterFrame *)push_chunk(tstate, (int)size);
}

void
_PyThreadState_PopFrame(PyThreadState *tstate, _PyInterpreterFrame * frame)
{
    assert(tstate->datastack_chunk);
    PyObject **base = (PyObject **)frame;
    if (base == &tstate->datastack_chunk->data[0]) {
        _PyStackChunk *chunk = tstate->datastack_chunk;
        _PyStackChunk *previous = chunk->previous;
        // push_chunk ensures that the root chunk is never popped:
        assert(previous);
        tstate->datastack_top = &previous->data[previous->top];
        tstate->datastack_chunk = previous;
        _PyObject_VirtualFree(chunk, chunk->size);
        tstate->datastack_limit = (PyObject **)(((char *)previous) + previous->size);
    }
    else {
        assert(tstate->datastack_top);
        assert(tstate->datastack_top >= base);
        tstate->datastack_top = base;
    }
}


#ifdef __cplusplus
}
#endif
