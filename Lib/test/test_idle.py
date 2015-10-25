import unittest
from test.support import import_module, import_fresh_module

# Skip test if _thread or _tkinter wasn't built or idlelib was deleted.
import_module('threading')  # imported by PyShell, imports _thread
tk = import_module('tkinter')  # imports _tkinter
idletest = import_module('idlelib.idle_test')
# Make sure TCL_LIBRARY is set properly on Windows.  Note that this will
# cause a warning about test_idle modifying the environment
import_fresh_module('tkinter._fix')

# Without test_main present, regrtest.runtest_inner (line1219) calls
# unittest.TestLoader().loadTestsFromModule(this_module) which calls
# load_tests() if it finds it. (Unittest.main does the same.)
load_tests = idletest.load_tests

if __name__ == '__main__':
    unittest.main(verbosity=2, exit=False)
