"""Tests for C-implemented GenericAlias."""

import unittest


class BaseTest(unittest.TestCase):
    """Test basics."""

    def test_subscriptable(self):
        for t in tuple, list, dict, set, frozenset:
            tname = t.__name__
            with self.subTest(f"Testing {tname}"):
                alias = t[int]
                self.assertIs(alias.__origin__, t)
                self.assertEqual(alias.__parameters__, (int,))

    def test_unsubscriptable(self):
        for t in int, str, float:
            tname = t.__name__
            with self.subTest(f"Testing {tname}"):
                with self.assertRaises(TypeError):
                    t[int]

    def test_instantiate(self):
        for t in tuple, list, dict, set, frozenset:
            tname = t.__name__
            with self.subTest(f"Testing {tname}"):
                alias = t[int]
                self.assertEqual(alias(), t())
                if t is dict:
                    self.assertEqual(alias(iter([('a', 1), ('b', 2)])), dict(a=1, b=2))
                    self.assertEqual(alias(a=1, b=2), dict(a=1, b=2))
                else:
                    self.assertEqual(alias(iter((1, 2, 3))), t((1, 2, 3)))

    def test_unbound_methods(self):
        t = list[int]
        a = t()
        t.append(a, 'foo')
        self.assertEqual(a, ['foo'])
        x = t.__getitem__(a, 0)
        self.assertEqual(x, 'foo')
        self.assertEqual(t.__len__(a), 1)


if __name__ == "__main__":
    unittest.main()
