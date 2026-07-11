--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec promotes to float once the value exceeds PHP_INT_MAX
--FILE--
<?php
// A value above PHP_INT_MAX is returned as a float (like PHP), not a wrapped or
// saturated int. is_float()/comparison are used instead of var_dump to dodge the
// var_dump float-precision divergence (a §3.7 fidelity item); the value itself is
// byte-exact with php.
$v = hexdec("ffffffffffffffff");   // 2^64 - 1
echo is_float($v) ? "float " : "int ";
echo $v > PHP_INT_MAX ? "big " : "notbig ";
echo ($v == 1.8446744073709552e19) ? "eq\n" : "ne\n";
echo hexdec("7fffffffffffffff"), "\n";  // PHP_INT_MAX: stays int
echo hexdec("ff"), "\n";                // small control
?>
--EXPECT--
float big eq
9223372036854775807
255
--CLEAN--
<?php
