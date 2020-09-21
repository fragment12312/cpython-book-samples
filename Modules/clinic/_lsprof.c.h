/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_lsprof_Profiler_getstats__doc__,
"getstats($self, /)\n"
"--\n"
"\n"
"list of profiler_entry objects.\n"
"\n"
"getstats() -> list of profiler_entry objects\n"
"\n"
"Return all information collected by the profiler.\n"
"Each profiler_entry is a tuple-like object with the\n"
"following attributes:\n"
"\n"
"    code          code object\n"
"    callcount     how many times this was called\n"
"    reccallcount  how many times called recursively\n"
"    totaltime     total time in this entry\n"
"    inlinetime    inline time in this entry (not in subcalls)\n"
"    calls         details of the calls\n"
"\n"
"The calls attribute is either None or a list of\n"
"profiler_subentry objects:\n"
"\n"
"    code          called code object\n"
"    callcount     how many times this is called\n"
"    reccallcount  how many times this is called recursively\n"
"    totaltime     total time spent in this call\n"
"    inlinetime    inline time (not in further subcalls)");

#define _LSPROF_PROFILER_GETSTATS_METHODDEF    \
    {"getstats", (PyCFunction)_lsprof_Profiler_getstats, METH_NOARGS, _lsprof_Profiler_getstats__doc__},

static PyObject *
_lsprof_Profiler_getstats_impl(ProfilerObject *self);

static PyObject *
_lsprof_Profiler_getstats(ProfilerObject *self, PyObject *Py_UNUSED(ignored))
{
    return _lsprof_Profiler_getstats_impl(self);
}
/*[clinic end generated code: output=24c525812713e00f input=a9049054013a1b77]*/
