#!/usr/bin/env python

import argparse
import glob
import os.path
import pathlib
import re
import subprocess
import sys
import sysconfig

EXCLUDED_HEADERS = {
    "bytes_methods.h",
    "cellobject.h",
    "classobject.h",
    "code.h",
    "compile.h",
    "datetime.h",
    "dtoa.h",
    "frameobject.h",
    "funcobject.h",
    "genobject.h",
    "longintrepr.h",
    "parsetok.h",
    "pyatomic.h",
    "pytime.h",
    "token.h",
    "ucnhash.h",
}

MACOS = (sys.platform == "darwin")

def get_exported_symbols(library, dynamic=False):
    # Only look at dynamic symbols
    args = ["nm", "--no-sort"]
    if dynamic:
        args.append("--dynamic")
    args.append(library)
    proc = subprocess.run(args, stdout=subprocess.PIPE, universal_newlines=True)
    if proc.returncode:
        sys.stdout.write(proc.stdout)
        sys.exit(proc.returncode)

    stdout = proc.stdout.rstrip()
    if not stdout:
        raise Exception("command output is empty")

    for line in stdout.splitlines():
        # Split line '0000000000001b80 D PyTextIOWrapper_Type'
        if not line:
            continue

        parts = line.split(maxsplit=2)
        if len(parts) < 3:
            continue

        symbol = parts[-1]
        if MACOS and symbol.startswith("_"):
            yield symbol[1:]
        else:
            yield symbol


def check_library(stable_abi_file, library, abi_funcs, dynamic=False):
    available_symbols = set(get_exported_symbols(library, dynamic))
    missing_symbols = abi_funcs - available_symbols
    if missing_symbols:
        raise Exception(
            f"""\
Some symbols from the limited API are missing: {', '.join(missing_symbols)}

This error means that there are some missing symbols among the ones exported
in the Python library ("libpythonx.x.a" or "libpythonx.x.so"). This normally
means that some symbol, function implementation or a prototype, belonging to
a symbol in the limited API has been deleted or is missing.

Check if this was a mistake and if not, update the file containing the limited
API symbols. This file is located at:

{stable_abi_file}

You can read more about the limited API and its contracts at:

https://docs.python.org/3/c-api/stable.html

And in PEP 384:

https://www.python.org/dev/peps/pep-0384/
"""
        )


def generate_limited_api_symbols(args):
    library = sysconfig.get_config_var("LIBRARY")
    ldlibrary = sysconfig.get_config_var("LDLIBRARY")
    if ldlibrary != library:
        raise Exception("Limited ABI symbols can only be generated from a static build")
    available_symbols = {
        symbol for symbol in get_exported_symbols(library) if symbol.startswith("Py")
    }

    headers = [
        file
        for file in pathlib.Path("Include").glob("*.h")
        if file.name not in EXCLUDED_HEADERS
    ]
    stable_data, stable_exported_data, stable_functions = get_limited_api_definitions(
        headers
    )

    stable_symbols = {
        symbol
        for symbol in (stable_functions | stable_exported_data | stable_data)
        if symbol.startswith("Py") and symbol in available_symbols
    }
    with open(args.output_file, "w") as output_file:
        output_file.write(f"# File generated by 'make regen-limited-abi'\n")
        output_file.write(
            f"# This is NOT an authoritative list of stable ABI symbols\n"
        )
        for symbol in sorted(stable_symbols):
            output_file.write(f"{symbol}\n")


def get_limited_api_definitions(headers):
    """Run the preprocesor over all the header files in "Include" setting
    "-DPy_LIMITED_API" to the correct value for the running version of the interpreter.

    The limited API symbols will be extracted from the output of this command as it includes
    the prototypes and definitions of all the exported symbols that are in the limited api.

    This function does *NOT* extract the macros defined on the limited API
    """
    preprocesor_output = subprocess.check_output(
        sysconfig.get_config_var("CC").split()
        + [
            # Prevent the expansion of the exported macros so we can capture them later
            "-DPyAPI_FUNC=__PyAPI_FUNC",
            "-DPyAPI_DATA=__PyAPI_DATA",
            "-DEXPORT_DATA=__EXPORT_DATA",
            "-D_Py_NO_RETURN=",
            "-DSIZEOF_WCHAR_T=4",  # The actual value is not important
            f"-DPy_LIMITED_API={sys.version_info.major << 24 | sys.version_info.minor << 16}",
            "-I.",
            "-I./Include",
            "-E",
        ]
        + [str(file) for file in headers],
        text=True,
        stderr=subprocess.DEVNULL,
    )
    stable_functions = set(
        re.findall(r"__PyAPI_FUNC\(.*?\)\s*(.*?)\s*\(", preprocesor_output)
    )
    stable_exported_data = set(
        re.findall(r"__EXPORT_DATA\((.*?)\)", preprocesor_output)
    )
    stable_data = set(
        re.findall(r"__PyAPI_DATA\(.*?\)\s*\(?(.*?)\)?\s*;", preprocesor_output)
    )
    return stable_data, stable_exported_data, stable_functions


def check_symbols(parser_args):
    with open(parser_args.stable_abi_file, "r") as filename:
        abi_funcs = {
            symbol
            for symbol in filename.read().splitlines()
            if symbol and not symbol.startswith("#")
        }

    try:
        # static library
        LIBRARY = sysconfig.get_config_var("LIBRARY")
        if not LIBRARY:
            raise Exception("failed to get LIBRARY variable from sysconfig")
        if os.path.exists(LIBRARY):
            check_library(parser_args.stable_abi_file, LIBRARY, abi_funcs)

        # dynamic library
        LDLIBRARY = sysconfig.get_config_var("LDLIBRARY")
        if not LDLIBRARY:
            raise Exception("failed to get LDLIBRARY variable from sysconfig")
        if LDLIBRARY != LIBRARY:
            check_library(
                parser_args.stable_abi_file, LDLIBRARY, abi_funcs, dynamic=True
            )
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Process some integers.")
    subparsers = parser.add_subparsers()
    check_parser = subparsers.add_parser(
        "check", help="Check the exported symbols against a given ABI file"
    )
    check_parser.add_argument(
        "stable_abi_file", type=str, help="File with the stable abi functions"
    )
    check_parser.set_defaults(func=check_symbols)
    generate_parser = subparsers.add_parser(
        "generate",
        help="Generate symbols from the header files and the exported symbols",
    )
    generate_parser.add_argument(
        "output_file", type=str, help="File to dump the symbols to"
    )
    generate_parser.set_defaults(func=generate_limited_api_symbols)
    args = parser.parse_args()
    if "func" not in args:
        parser.error("Either 'check' or 'generate' must be used")
        sys.exit(1)

    args.func(args)


if __name__ == "__main__":
    main()
