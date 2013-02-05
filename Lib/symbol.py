#! /usr/bin/env python3

"""Non-terminal symbols of Python grammar (from "graminit.h")."""

#  This file is automatically generated; please don't muck it up!
#
#  To update the symbols in this file, 'cd' to the top directory of
#  the python source tree after building the interpreter and run:
#
#    ./python Lib/symbol.py

#--start constants--
single_input = 256
file_input = 257
eval_input = 258
decorator = 259
decorators = 260
decorated = 261
funcdef = 262
parameters = 263
typedargslist = 264
tfpdef = 265
varargslist = 266
vfpdef = 267
stmt = 268
simple_stmt = 269
small_stmt = 270
expr_stmt = 271
testlist_star_expr = 272
augassign = 273
del_stmt = 274
pass_stmt = 275
flow_stmt = 276
break_stmt = 277
continue_stmt = 278
return_stmt = 279
yield_stmt = 280
raise_stmt = 281
import_stmt = 282
import_name = 283
import_from = 284
import_as_name = 285
dotted_as_name = 286
import_as_names = 287
dotted_as_names = 288
dotted_name = 289
global_stmt = 290
nonlocal_stmt = 291
assert_stmt = 292
compound_stmt = 293
if_stmt = 294
while_stmt = 295
for_stmt = 296
try_stmt = 297
with_stmt = 298
with_item = 299
except_clause = 300
suite = 301
test = 302
test_nocond = 303
lambdef = 304
lambdef_nocond = 305
or_test = 306
and_test = 307
not_test = 308
comparison = 309
comp_op = 310
star_expr = 311
expr = 312
xor_expr = 313
and_expr = 314
shift_expr = 315
arith_expr = 316
term = 317
factor = 318
power = 319
atom = 320
testlist_comp = 321
trailer = 322
subscriptlist = 323
subscript = 324
sliceop = 325
exprlist = 326
testlist = 327
dictorsetmaker = 328
classdef = 329
arglist = 330
argument = 331
comp_iter = 332
comp_for = 333
comp_if = 334
encoding_decl = 335
yield_expr = 336
#--end constants--

sym_name = {}
for _name, _value in list(globals().items()):
    if type(_value) is type(0):
        sym_name[_value] = _name


def main():
    import sys
    import token
    if len(sys.argv) == 1:
        sys.argv = sys.argv + ["Include/graminit.h", "Lib/symbol.py"]
    token._main()

if __name__ == "__main__":
    main()
