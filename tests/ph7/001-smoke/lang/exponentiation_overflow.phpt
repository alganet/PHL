--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exponentiation: integer overflow promotes to float
--FILE--
<?php
// 2^62 fits in signed int64: stays int
echo 2 ** 62, "\n";
echo is_int(2 ** 62) ? "int\n" : "float\n";

// 2^63 overflows signed int64: becomes float
echo is_float(2 ** 63) ? "float\n" : "int\n";

// Large base squared overflows
echo is_float(PHP_INT_MAX ** 2) ? "float\n" : "int\n";

// Float operand makes result float even when magnitude fits
echo is_float(2.0 ** 3) ? "float\n" : "int\n";
echo is_float(2 ** 3.0) ? "float\n" : "int\n";

// Integer stays integer when both operands are int and exponent >= 0
echo is_int(3 ** 10) ? "int\n" : "float\n";
echo is_int(7 ** 7) ? "int\n" : "float\n";
?>
--EXPECT--
4611686018427387904
int
float
float
float
float
int
int
