--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP 8.5: round() edge values (INF/NAN/int) and argument errors
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.5.0', '<')) echo 'skip Requires PHP 8.5+'; ?>
--FILE--
<?php
// Non-finite and identity edge cases.
printf("INF=%s -INF=%s NAN=%s\n", var_export(round(INF), true), var_export(round(-INF), true), var_export(round(NAN), true));
printf("zero=%s neg_zero=%s\n", var_export(round(0.0), true), var_export(round(-0.0), true));
// Integer input returns a float; negative precision still rounds it.
printf("int5=%s int5_p2=%s int5_pm1=%s\n", var_export(round(5), true), var_export(round(5, 2), true), var_export(round(5, -1), true));
// Numeric string is accepted (coerced).
printf("str=%s\n", var_export(round("1.5"), true));

// Argument errors (byte-exact messages).
function t($f){ try { var_export($f()); echo "\n"; } catch(\Throwable $e){ echo get_class($e), ': ', $e->getMessage(), "\n"; } }
t(fn() => round([1, 2]));
t(fn() => round("not a number"));
t(fn() => round(1, 2, 3, 4));
t(fn() => round(1.5, 0, 99));
t(fn() => round(1.5, 0, 0));
?>
--EXPECT--
INF=INF -INF=-INF NAN=NAN
zero=0.0 neg_zero=-0.0
int5=5.0 int5_p2=5.0 int5_pm1=10.0
str=2.0
TypeError: round(): Argument #1 ($num) must be of type int|float, array given
TypeError: round(): Argument #1 ($num) must be of type int|float, string given
ArgumentCountError: round() expects at most 3 arguments, 4 given
ValueError: round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)
ValueError: round(): Argument #3 ($mode) must be a valid rounding mode (RoundingMode::*)
