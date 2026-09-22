--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a float outside the int64 range casts to PHP_INT_MIN, deterministically (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* A float outside the int64 range converts to PHP_INT_MIN here, in ONE place
 * (MemObjRealToInt) and for every direction, including NaN and both infinities.
 * That is the value x86's own out-of-range cast produced, kept deliberately -- but
 * it is now REACHED deliberately too: the upper bound is tested against +2^63
 * (exact in double space) instead of against (double)PHP_INT_MAX, which rounds UP
 * to 2^63 and let a double of exactly that value fall through to a C cast whose
 * result is undefined -- x86 answered PHP_INT_MIN there, aarch64 saturates to
 * PHP_INT_MAX, so the same expression answered two different things on the two
 * platforms PHL builds for. The largest and smallest doubles that DO fit convert
 * exactly, and everything in range is untouched.
 *
 * php's answer is a modular wrap (0 for NaN/infinity) behind
 * `The float X is not representable as an int, cast occurred` -- see the _zend
 * half. The divergence is recorded; this half pins that PHL is at least
 * deterministic and platform-independent about it. */
set_error_handler(function ($n, $s) { echo "  W: $s\n"; return true; });
$cases = [
    "2**63 exactly"   => 9.2233720368547758E+18,
    "-(2**63)"        => -9.2233720368547758E+18,
    "largest in "     => 9223372036854774784.0,
    "smallest in"     => -9223372036854775808.0,
    "1e19"            => 1.0E+19,
    "1e30"            => 1.0E+30,
    "NAN"             => NAN,
    "INF"             => INF,
    "-INF"            => -INF,
    "0.0"             => 0.0,
    "1.9"             => 1.9,
    "-1.9"            => -1.9,
];
foreach ($cases as $label => $f) {
    echo $label, "\n";
    var_dump((int)$f);
    var_dump(intval($f));
}
restore_error_handler();
?>
--EXPECT--
2**63 exactly
int(-9223372036854775808)
int(-9223372036854775808)
-(2**63)
int(-9223372036854775808)
int(-9223372036854775808)
largest in 
int(9223372036854774784)
int(9223372036854774784)
smallest in
int(-9223372036854775808)
int(-9223372036854775808)
1e19
int(-9223372036854775808)
int(-9223372036854775808)
1e30
int(-9223372036854775808)
int(-9223372036854775808)
NAN
int(-9223372036854775808)
int(-9223372036854775808)
INF
int(-9223372036854775808)
int(-9223372036854775808)
-INF
int(-9223372036854775808)
int(-9223372036854775808)
0.0
int(0)
int(0)
1.9
int(1)
int(1)
-1.9
int(-1)
int(-1)
--CLEAN--
<?php
