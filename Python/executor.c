#include "Python.h"

#include "opcode.h"

#include "pycore_bitutils.h"
#include "pycore_call.h"
#include "pycore_ceval.h"
#include "pycore_dict.h"
#include "pycore_emscripten_signal.h"
#include "pycore_intrinsics.h"
#include "pycore_long.h"
#include "pycore_object.h"
#include "pycore_opcode_metadata.h"
#include "pycore_opcode_utils.h"
#include "pycore_pyerrors.h"
#include "pycore_range.h"
#include "pycore_setobject.h"     // _PySet_Update()
#include "pycore_sliceobject.h"
#include "pycore_uops.h"

#define TIER_TWO 2
#include "ceval_macros.h"

#undef GOTO_ERROR
#define GOTO_ERROR(LABEL) goto LABEL ## _tier_two

#undef DEOPT_IF
#define DEOPT_IF(COND, INSTNAME) \
    if ((COND)) {                \
        goto deoptimize_tier_two;\
    }

#ifdef Py_STATS
// Disable these macros that apply to Tier 1 stats when we are in Tier 2
#undef STAT_INC
#define STAT_INC(opname, name) ((void)0)
#undef STAT_DEC
#define STAT_DEC(opname, name) ((void)0)
#undef CALL_STAT_INC
#define CALL_STAT_INC(name) ((void)0)
#endif

#undef ENABLE_SPECIALIZATION
#define ENABLE_SPECIALIZATION 0


_PyInterpreterFrame *
_PyUopExecute(_PyExecutorObject *executor, _PyInterpreterFrame *frame, PyObject **stack_pointer)
{
    Py_FatalError("Tier 2 is now inlined into Tier 1");
#ifdef Py_DEBUG
    char *python_lltrace = Py_GETENV("PYTHON_LLTRACE");
    int lltrace = 0;
    if (python_lltrace != NULL && *python_lltrace >= '0') {
        lltrace = *python_lltrace - '0';  // TODO: Parse an int and all that
    }
    #define DPRINTF(level, ...) \
        if (lltrace >= (level)) { printf(__VA_ARGS__); }
#else
    #define DPRINTF(level, ...)
#endif

    DPRINTF(3,
            "Entering _PyUopExecute for %s (%s:%d) at byte offset %ld\n",
            PyUnicode_AsUTF8(_PyFrame_GetCode(frame)->co_qualname),
            PyUnicode_AsUTF8(_PyFrame_GetCode(frame)->co_filename),
            _PyFrame_GetCode(frame)->co_firstlineno,
            2 * (long)(frame->instr_ptr -
                   (_Py_CODEUNIT *)_PyFrame_GetCode(frame)->co_code_adaptive));

    PyThreadState *tstate = _PyThreadState_GET();
    _PyUOpExecutorObject *self = (_PyUOpExecutorObject *)executor;

    CHECK_EVAL_BREAKER();

    OPT_STAT_INC(traces_executed);
    _Py_CODEUNIT *ip_offset = (_Py_CODEUNIT *)_PyFrame_GetCode(frame)->co_code_adaptive;
    _PyUOpInstruction *next_uop = self->trace;
    int opcode;
    int oparg;
    uint64_t operand;
#ifdef Py_STATS
    uint64_t trace_uop_execution_counter = 0;
#endif

    for (;;) {
        opcode = next_uop->opcode;
        oparg = next_uop->oparg;
        operand = next_uop->operand;
        DPRINTF(3,
                "%4d: uop %s, oparg %d, operand %" PRIu64 ", stack_level %d\n",
                (int)(next_uop - self->trace),
                opcode < 256 ? _PyOpcode_OpName[opcode] : _PyOpcode_uop_name[opcode],
                oparg,
                operand,
                (int)(stack_pointer - _PyFrame_Stackbase(frame)));
        next_uop++;
        OPT_STAT_INC(uops_executed);
        UOP_EXE_INC(opcode);
#ifdef Py_STATS
        trace_uop_execution_counter++;
#endif

        switch (opcode) {

#include "executor_cases.c.h"

            default:
            {
                fprintf(stderr, "Unknown uop %d, operand %" PRIu64 "\n", opcode, operand);
                Py_FatalError("Unknown uop");
            }

        }
    }

unbound_local_error_tier_two:
    _PyEval_FormatExcCheckArg(tstate, PyExc_UnboundLocalError,
        UNBOUNDLOCAL_ERROR_MSG,
        PyTuple_GetItem(_PyFrame_GetCode(frame)->co_localsplusnames, oparg)
    );
    goto error_tier_two;

pop_4_error_tier_two:
    STACK_SHRINK(1);
pop_3_error_tier_two:
    STACK_SHRINK(1);
pop_2_error_tier_two:
    STACK_SHRINK(1);
pop_1_error_tier_two:
    STACK_SHRINK(1);
error_tier_two:
    // On ERROR_IF we return NULL as the frame.
    // The caller recovers the frame from tstate->current_frame.
    DPRINTF(2, "Error: [Opcode %d, operand %" PRIu64 "]\n", opcode, operand);
    OPT_HIST(trace_uop_execution_counter, trace_run_length_hist);
    frame->return_offset = 0;  // Don't leave this random
    _PyFrame_SetStackPointer(frame, stack_pointer);
    Py_DECREF(self);
    return NULL;

deoptimize_tier_two:
    // On DEOPT_IF we just repeat the last instruction.
    // This presumes nothing was popped from the stack (nor pushed).
    DPRINTF(2, "DEOPT: [Opcode %d, operand %" PRIu64 "]\n", opcode, operand);
    OPT_HIST(trace_uop_execution_counter, trace_run_length_hist);
    frame->return_offset = 0;  // Dispatch to frame->instr_ptr
    _PyFrame_SetStackPointer(frame, stack_pointer);
    Py_DECREF(self);
enter_tier_one:
    return frame;
}
