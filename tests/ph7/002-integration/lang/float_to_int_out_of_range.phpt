--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A float outside the int64 range wraps and warns at every cast site
--FILE--
<?php
/* php's cast of a float no int can hold: a modular WRAP of its low 64 bits (0
 * for NaN, both infinities, and any magnitude whose every set bit sits above
 * the 64th), behind an E_WARNING naming the value. PHL answered PHP_INT_MIN
 * for all of them and said nothing -- one conversion and one diagnostic, both
 * closed in the 70th session.
 *
 * The warning is php's CAST diagnostic, not its lossy-conversion DEPRECATION:
 * an explicit cast never deprecates, and the implicit sites that do (`1e19|0`,
 * `$a[1e19]`) are §10's TypeError here, before any cast happens. */
set_error_handler(function ($n, $s) { echo "  W($n): $s\n"; return true; });
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
/* The same warning, from the other cast sites: settype(), the printf integer
 * conversions (each argument its own), and a native subscript that reads an
 * int out of its offset. A numeric STRING is a different conversion entirely --
 * php SATURATES that one and says nothing, however wide it is. */
$wide = 1.0E+19;
settype($wide, 'int');
var_dump($wide);
var_dump(bin2hex(sprintf('%d %x %c', 1.0E+19, 1.0E+19, 1.0E+19)));
var_dump((int)"1e19", (int)"99999999999999999999", intval("1e400"));
restore_error_handler();
?>
--EXPECT--
2**63 exactly
  W(2): The float 9.223372036854776E+18 is not representable as an int, cast occurred
int(-9223372036854775808)
  W(2): The float 9.223372036854776E+18 is not representable as an int, cast occurred
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
  W(2): The float 1.0E+19 is not representable as an int, cast occurred
int(-8446744073709551616)
  W(2): The float 1.0E+19 is not representable as an int, cast occurred
int(-8446744073709551616)
1e30
  W(2): The float 1.0E+30 is not representable as an int, cast occurred
int(5076964154930102272)
  W(2): The float 1.0E+30 is not representable as an int, cast occurred
int(5076964154930102272)
NAN
  W(2): The float NAN is not representable as an int, cast occurred
int(0)
  W(2): The float NAN is not representable as an int, cast occurred
int(0)
INF
  W(2): The float INF is not representable as an int, cast occurred
int(0)
  W(2): The float INF is not representable as an int, cast occurred
int(0)
-INF
  W(2): The float -INF is not representable as an int, cast occurred
int(0)
  W(2): The float -INF is not representable as an int, cast occurred
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
  W(2): The float 1.0E+19 is not representable as an int, cast occurred
int(-8446744073709551616)
  W(2): The float 1.0E+19 is not representable as an int, cast occurred
  W(2): The float 1.0E+19 is not representable as an int, cast occurred
  W(2): The float 1.0E+19 is not representable as an int, cast occurred
string(78) "2d3834343637343430373337303935353136313620386163373233303438396538303030302000"
int(9223372036854775807)
int(9223372036854775807)
int(0)
--CLEAN--
<?php
