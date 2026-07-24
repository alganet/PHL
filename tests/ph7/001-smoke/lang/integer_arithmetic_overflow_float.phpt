--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Integer arithmetic that overflows the signed 64-bit range promotes to float
--FILE--
<?php
/* PHP promotes an integer +, -, *, ++ or -- whose exact result exceeds the
 * signed 64-bit range to a float instead of wrapping. The int/float
 * classification and the resulting value are byte-identical to php 8.5.7. The
 * promoted values are asserted with === against the equivalent overflow
 * literals (which php parses as the same doubles); var_dump is avoided because
 * php's serialize-precision float shape is a separate tracked divergence. */
function tyArith($v) { return is_float($v) ? "float" : (is_int($v) ? "int" : "?"); }

// Addition
echo tyArith(PHP_INT_MAX + 1), "\n";                 // float
echo (PHP_INT_MAX + 1) === 9223372036854775808 ? "add_ok\n" : "add_bad\n";
echo tyArith(PHP_INT_MAX + PHP_INT_MAX), "\n";       // float
echo tyArith(PHP_INT_MAX + 0), "\n";                 // int (no overflow)
echo tyArith(1000 + 2000), "\n";                     // int

// Subtraction
echo tyArith(PHP_INT_MIN - 1), "\n";                 // float
echo (PHP_INT_MIN - 1) === -9223372036854775809 ? "sub_ok\n" : "sub_bad\n";
echo tyArith(PHP_INT_MIN - PHP_INT_MAX), "\n";       // float
echo tyArith(5 - 9), "\n";                           // int

// Multiplication
echo tyArith(PHP_INT_MAX * 2), "\n";                 // float
echo (PHP_INT_MAX * 2) === 18446744073709551616 ? "mul_ok\n" : "mul_bad\n";
echo tyArith(4611686018427387904 * 2), "\n";         // 2^62 * 2 == 2^63 -> float
echo tyArith(PHP_INT_MAX * PHP_INT_MAX), "\n";       // float
echo tyArith(PHP_INT_MAX * 1), "\n";                 // int (no overflow)
echo tyArith(-1 * PHP_INT_MIN), "\n";                // float (|min| overflows)
echo tyArith(6 * 7), "\n";                           // int

// Unary minus on PHP_INT_MIN (its magnitude overflows)
echo tyArith(-PHP_INT_MIN), "\n";                    // float
echo (-PHP_INT_MIN) === 9223372036854775808 ? "neg_ok\n" : "neg_bad\n";
echo tyArith(-PHP_INT_MAX), "\n";                    // int (fits)

// Increment / decrement
$a = PHP_INT_MAX; $a++; echo tyArith($a), "\n";      // float
echo $a === 9223372036854775808 ? "incr_ok\n" : "incr_bad\n";
$b = PHP_INT_MIN; $b--; echo tyArith($b), "\n";      // float
echo $b === -9223372036854775809 ? "decr_ok\n" : "decr_bad\n";
$c = 41; $c++; echo tyArith($c), " ", $c, "\n";      // int 42 (no overflow)

// Post-increment value semantics at the boundary: old value is still the int
$d = PHP_INT_MAX;
$old = $d++;
echo tyArith($old), " ", tyArith($d), "\n";               // int float

// Compound assignment overflows too
$e = PHP_INT_MAX; $e += 1; echo tyArith($e), "\n";   // float
$f = PHP_INT_MAX; $f *= 2; echo tyArith($f), "\n";   // float
$g = PHP_INT_MIN; $g -= 1; echo tyArith($g), "\n";   // float

// In-range compound assignment stays int
$h = 10; $h += 5; $h *= 3; echo tyArith($h), " ", $h, "\n"; // int 45

// Modulo at the INT_MIN / -1 boundary: `a % -1` is 0 for every a. Computing it
// as a%b would trap (SIGFPE) on x86 for PHP_INT_MIN, so it is special-cased.
echo (PHP_INT_MIN % -1), " ", (7 % -1), " ", (PHP_INT_MIN % -2), "\n"; // 0 0 0
$m = PHP_INT_MIN; $m %= -1; echo $m, "\n";                             // 0
?>
--EXPECT--
float
add_ok
float
int
int
float
sub_ok
float
int
float
mul_ok
float
float
int
float
int
float
neg_ok
int
float
incr_ok
float
decr_ok
int 42
int float
float
float
float
int 45
0 0 0
0
--CLEAN--
<?php
