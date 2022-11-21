import unittest
from test import support

try:
    import _testcapi
except ImportError:
    _testcapi = None


class CAPITest(unittest.TestCase):

    @support.cpython_only
    @unittest.skipIf(_testcapi is None, 'need _testcapi module')
    def test_decodeutf8(self):
        """Test PyUnicode_DecodeUTF8()"""
        from _testcapi import unicode_decodeutf8 as decodeutf8

        for s in ['abc', '\xa1\xa2', '\u4f60\u597d', 'a\U0001f600']:
            b = s.encode('utf-8')
            self.assertEqual(decodeutf8(b), s)
            self.assertEqual(decodeutf8(b, 'strict'), s)

        self.assertRaises(UnicodeDecodeError, decodeutf8, b'\x80')
        self.assertRaises(UnicodeDecodeError, decodeutf8, b'\xc0')
        self.assertRaises(UnicodeDecodeError, decodeutf8, b'\xff')
        self.assertRaises(UnicodeDecodeError, decodeutf8, b'a\xf0\x9f')
        self.assertEqual(decodeutf8(b'a\xf0\x9f', 'replace'), 'a\ufffd')
        self.assertEqual(decodeutf8(b'a\xf0\x9fb', 'replace'), 'a\ufffdb')

        self.assertRaises(LookupError, decodeutf8, b'a\x80', 'foo')
        # TODO: Test PyUnicode_DecodeUTF8() with NULL as data and
        # negative size.

    @support.cpython_only
    @unittest.skipIf(_testcapi is None, 'need _testcapi module')
    def test_decodeutf8stateful(self):
        """Test PyUnicode_DecodeUTF8Stateful()"""
        from _testcapi import unicode_decodeutf8stateful as decodeutf8stateful

        for s in ['abc', '\xa1\xa2', '\u4f60\u597d', 'a\U0001f600']:
            b = s.encode('utf-8')
            self.assertEqual(decodeutf8stateful(b), (s, len(b)))
            self.assertEqual(decodeutf8stateful(b, 'strict'), (s, len(b)))

        self.assertRaises(UnicodeDecodeError, decodeutf8stateful, b'\x80')
        self.assertRaises(UnicodeDecodeError, decodeutf8stateful, b'\xc0')
        self.assertRaises(UnicodeDecodeError, decodeutf8stateful, b'\xff')
        self.assertEqual(decodeutf8stateful(b'a\xf0\x9f'), ('a', 1))
        self.assertEqual(decodeutf8stateful(b'a\xf0\x9f', 'replace'), ('a', 1))
        self.assertRaises(UnicodeDecodeError, decodeutf8stateful, b'a\xf0\x9fb')
        self.assertEqual(decodeutf8stateful(b'a\xf0\x9fb', 'replace'), ('a\ufffdb', 4))

        self.assertRaises(LookupError, decodeutf8stateful, b'a\x80', 'foo')
        # TODO: Test PyUnicode_DecodeUTF8Stateful() with NULL as data and
        # negative size.
        # TODO: Test PyUnicode_DecodeUTF8Stateful() with NULL as the address of
        # "consumed".


if __name__ == "__main__":
    unittest.main()
