# This script generates the pycore_opcode.h header file.

import sys
import tokenize

SCRIPT_NAME = "Tools/build/generate_opcode_h.py"
PYTHON_OPCODE = "Lib/opcode.py"

internal_header = f"""
// Auto-generated by {SCRIPT_NAME} from {PYTHON_OPCODE}

#ifndef Py_INTERNAL_OPCODE_H
#define Py_INTERNAL_OPCODE_H
#ifdef __cplusplus
extern "C" {{
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "opcode.h"
""".lstrip()

internal_footer = """
#ifdef __cplusplus
}
#endif
#endif  // !Py_INTERNAL_OPCODE_H
"""

DEFINE = "#define {:<38} {:>3}\n"

UINT32_MASK = (1<<32)-1

def get_python_module_dict(filename):
    mod = {}
    with tokenize.open(filename) as fp:
        code = fp.read()
    exec(code, mod)
    return mod

def main(opcode_py,
         internal_opcode_h='Include/internal/pycore_opcode.h'):

    opcode = get_python_module_dict(opcode_py)

    with open(internal_opcode_h, 'w') as iobj:
        iobj.write(internal_header)

        iobj.write("\nextern const uint8_t _PyOpcode_Caches[256];\n")
        iobj.write("\n#ifdef NEED_OPCODE_TABLES\n")

        iobj.write("\nconst uint8_t _PyOpcode_Caches[256] = {\n")
        for name, entries in opcode["_inline_cache_entries"].items():
            iobj.write(f"    [{name}] = {entries},\n")
        iobj.write("};\n")

        iobj.write("#endif   // NEED_OPCODE_TABLES\n")

        iobj.write(internal_footer)

    print(f"{internal_opcode_h} regenerated from {opcode_py}")


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
