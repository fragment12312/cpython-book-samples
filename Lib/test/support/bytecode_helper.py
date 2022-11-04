"""bytecode_helper - support tools for testing correct bytecode generation"""

import unittest
import dis
import io
from _testinternalcapi import compiler_codegen, optimize_cfg

_UNSPECIFIED = object()

class BytecodeTestCase(unittest.TestCase):
    """Custom assertion methods for inspecting bytecode."""

    def get_disassembly_as_string(self, co):
        s = io.StringIO()
        dis.dis(co, file=s)
        return s.getvalue()

    def assertInBytecode(self, x, opname, argval=_UNSPECIFIED):
        """Returns instr if opname is found, otherwise throws AssertionError"""
        self.assertIn(opname, dis.opmap)
        for instr in dis.get_instructions(x):
            if instr.opname == opname:
                if argval is _UNSPECIFIED or instr.argval == argval:
                    return instr
        disassembly = self.get_disassembly_as_string(x)
        if argval is _UNSPECIFIED:
            msg = '%s not found in bytecode:\n%s' % (opname, disassembly)
        else:
            msg = '(%s,%r) not found in bytecode:\n%s'
            msg = msg % (opname, argval, disassembly)
        self.fail(msg)

    def assertNotInBytecode(self, x, opname, argval=_UNSPECIFIED):
        """Throws AssertionError if opname is found"""
        self.assertIn(opname, dis.opmap)
        for instr in dis.get_instructions(x):
            if instr.opname == opname:
                disassembly = self.get_disassembly_as_string(x)
                if argval is _UNSPECIFIED:
                    msg = '%s occurs in bytecode:\n%s' % (opname, disassembly)
                    self.fail(msg)
                elif instr.argval == argval:
                    msg = '(%s,%r) occurs in bytecode:\n%s'
                    msg = msg % (opname, argval, disassembly)
                    self.fail(msg)

class CompilationStepTestCase(unittest.TestCase):

    HAS_ARG = set(dis.hasarg)
    HAS_TARGET = set(dis.hasjrel + dis.hasjabs + dis.hasexc)
    HAS_ARG_OR_TARGET = HAS_ARG.union(HAS_TARGET)

    def setUp(self):
        self.last_label = 0

    def Label(self):
        self.last_label += 1
        return self.last_label

    def compareInstructions(self, actual_, expected_):
        # get two lists where each entry is a label or
        # an instruction tuple. Compare them, while mapping
        # each actual label to a corresponding expected label
        # based on their locations.

        self.assertIsInstance(actual_, list)
        self.assertIsInstance(expected_, list)

        actual = self.normalize_insts(actual_)
        expected = self.normalize_insts(expected_)
        self.assertEqual(len(actual), len(expected))

        # compare instructions
        for act, exp in zip(actual, expected):
            if isinstance(act, int):
                self.assertEqual(exp, act)
                continue
            self.assertIsInstance(exp, tuple)
            self.assertIsInstance(act, tuple)
            # crop comparison to the provided expected values
            if len(act) > len(exp):
                act = act[:len(exp)]
            self.assertEqual(exp, act)

    def normalize_insts(self, insts):
        """ Map labels to instruction index.
            Remove labels which are not used as jump targets.
            Map opcodes to opnames.
        """
        labels_map = {}
        targets = set()
        idx = 1
        for item in insts:
            assert isinstance(item, (int, tuple))
            if isinstance(item, tuple):
                opcode, oparg, *_ = item
                if dis.opmap.get(opcode, opcode) in self.HAS_TARGET:
                    targets.add(oparg)
                idx += 1
            elif isinstance(item, int):
                assert item not in labels_map, "label reused"
                labels_map[item] = idx

        res = []
        for item in insts:
            if isinstance(item, int) and item in targets:
                if not res or labels_map[item] != res[-1]:
                    res.append(labels_map[item])
            elif isinstance(item, tuple):
                opcode, oparg, *loc = item
                opcode = dis.opmap.get(opcode, opcode)
                if opcode in self.HAS_TARGET:
                    arg = labels_map[oparg]
                else:
                    arg = oparg if opcode in self.HAS_TARGET else None
                opcode = dis.opname[opcode]
                res.append((opcode, arg, *loc))
        return res


class CodegenTestCase(CompilationStepTestCase):

    def generate_code(self, ast):
        insts = compiler_codegen(ast, "my_file.py", 0)
        return insts


class CfgOptimizationTestCase(CompilationStepTestCase):

    def complete_insts_info(self, insts):
        # fill in omitted fields in location, and oparg 0 for ops with no arg.
        instructions = []
        for item in insts:
            if isinstance(item, int):
                instructions.append(item)
            else:
                assert isinstance(item, tuple)
                inst = list(reversed(item))
                opcode = dis.opmap[inst.pop()]
                oparg = inst.pop() if opcode in self.HAS_ARG_OR_TARGET else 0
                loc = inst + [-1] * (4 - len(inst))
                instructions.append((opcode, oparg, *loc))
        return instructions

    def get_optimized(self, insts, consts):
        insts = self.complete_insts_info(insts)
        insts = optimize_cfg(insts, consts)
        return insts, consts


