--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
int ** int is php's own doubling loop, not a call to pow()
--DESCRIPTION--
php computes `int ** non-negative int` in `pow_function_base`'s doubling loop and, the moment
a step OVERFLOWS, finishes in DOUBLE space FROM THERE — the overflowing product times
`pow(base, whatever exponent is left)` — rather than re-computing `pow(base, exp)` from the
original operands. PHL called pow() on the operands, and the answer differed in two ways:

the accumulated SIGN was lost, so `(-3) ** PHP_INT_MAX` was +INF where php answers -INF (an
odd exponent too large to be odd as a double); and the rounding differed, so `3 ** 100` was
5.1537752073201132e+47 against php's ...141e+47 and `10 ** 64` was exactly 1e+64 against
php's 1.0000000000000002e+64. 33 rows of a 380-row base × exponent sweep, sign included.

`pow()` IS the operator in php — both compile to the same function — so the builtin and
`**=` answer the same thing here as well.
--FILE--
<?php
echo "-- the sign the loop carries\n";
var_dump((-3) ** PHP_INT_MAX, (-2) ** PHP_INT_MAX, (-3) ** (PHP_INT_MAX - 1));
var_dump(3 ** PHP_INT_MAX, (-1) ** PHP_INT_MAX, (-1) ** (PHP_INT_MAX - 1));

echo "-- where the overflow hands over to doubles\n";
printf("%.17g\n", 3 ** 100);
printf("%.17g\n", (-3) ** 100);
printf("%.17g\n", 10 ** 64);
printf("%.17g\n", 10 ** 100);
printf("%.17g\n", (-171) ** 30);
printf("%.17g\n", (-155) ** 34);
printf("%.17g\n", 2 ** 1000);
printf("%.17g\n", (-210) ** 57);

echo "-- and where it stays an int\n";
var_dump(2 ** 62, 2 ** 63, (-2) ** 63, 0 ** 0, 0 ** 5, 1 ** PHP_INT_MAX, (-1) ** 3);
var_dump(2 ** 10, 10 ** 18, 10 ** 19);

echo "-- pow() and **= are the same operator\n";
$powA = -171; $powA **= 30;
var_dump($powA === (-171) ** 30, pow(-171, 30) === (-171) ** 30);
$powB = 2; $powB **= 62;
var_dump($powB, pow(2, 62) === 2 ** 62);
var_dump(pow(-3, PHP_INT_MAX) === (-3) ** PHP_INT_MAX);

echo "-- a float operand is still plain pow()\n";
printf("%.17g\n", 2.0 ** 100);
printf("%.17g\n", 2 ** 0.5);
var_dump(4 ** -1, is_float(4 ** -1));
?>
--EXPECT--
-- the sign the loop carries
float(-INF)
float(-INF)
float(INF)
float(INF)
int(-1)
int(1)
-- where the overflow hands over to doubles
5.1537752073201141e+47
5.1537752073201141e+47
1.0000000000000002e+64
1.0000000000000002e+100
9.7697468764337793e+66
2.9599047645948364e+74
1.0715086071862673e+301
-2.3254114140895377e+132
-- and where it stays an int
int(4611686018427387904)
float(9.223372036854776E+18)
int(-9223372036854775808)
int(1)
int(0)
int(1)
int(-1)
int(1024)
int(1000000000000000000)
float(1.0E+19)
-- pow() and **= are the same operator
bool(true)
bool(true)
int(4611686018427387904)
bool(true)
bool(true)
-- a float operand is still plain pow()
1.2676506002282294e+30
1.4142135623730951
float(0.25)
bool(true)
