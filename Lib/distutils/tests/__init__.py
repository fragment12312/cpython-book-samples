"""Test suite for distutils.

This test suite consists of a collection of test modules in the
distutils.tests package.  Each test module has a name starting with
'test' and contains a function test_suite().  The function is expected
to return an initialized unittest.TestSuite instance.

Tests for the command classes in the distutils.command package are
included in distutils.tests as well, instead of using a separate
distutils.command.tests package, since command identification is done
by import rather than matching pre-defined names.

"""

import os
import sys
import unittest
from test.support import run_unittest


# bpo-40055: Prevent docutils from being imported to avoid depending on
# docutils. Import docutils can have side effects. For example, docutils
# imports pkg_resources which changes warnings filters.
sys.modules['docutils'] = None


here = os.path.dirname(__file__) or os.curdir


def test_suite():
    suite = unittest.TestSuite()
    for fn in os.listdir(here):
        if fn.startswith("test") and fn.endswith(".py"):
            modname = "distutils.tests." + fn[:-3]
            __import__(modname)
            module = sys.modules[modname]
            suite.addTest(module.test_suite())
    return suite


if __name__ == "__main__":
    run_unittest(test_suite())
