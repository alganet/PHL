--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a float outside the int64 range wraps modularly and warns (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's non-representable float->int cast: a modular wrap, 0 for NaN and the
 * infinities, and an E_WARNING naming the value at every cast site. PHL answers
 * PHP_INT_MIN with no warning -- see the PHL half of the pair. */
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
  W: The float 9.223372036854776E+18 is not representable as an int, cast occurred
int(-9223372036854775808)
  W: The float 9.223372036854776E+18 is not representable as an int, cast occurred
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
  W: The float 1.0E+19 is not representable as an int, cast occurred
int(-8446744073709551616)
  W: The float 1.0E+19 is not representable as an int, cast occurred
int(-8446744073709551616)
1e30
  W: The float 1.0E+30 is not representable as an int, cast occurred
int(5076964154930102272)
  W: The float 1.0E+30 is not representable as an int, cast occurred
int(5076964154930102272)
NAN
  W: The float NAN is not representable as an int, cast occurred
int(0)
  W: The float NAN is not representable as an int, cast occurred
int(0)
INF
  W: The float INF is not representable as an int, cast occurred
int(0)
  W: The float INF is not representable as an int, cast occurred
int(0)
-INF
  W: The float -INF is not representable as an int, cast occurred
int(0)
  W: The float -INF is not representable as an int, cast occurred
int(0)
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
