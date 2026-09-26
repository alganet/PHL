--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A float no int can hold wraps modulo 2**64 at every cast site
--FILE--
<?php
/* php's non-representable float->int cast is a MODULAR WRAP, not a saturation:
 * the low 64 bits of the value survive and are read as a signed int. NaN and
 * both infinities are 0, and so is any magnitude whose every set bit sits above
 * the 64th (1e100 is one). One conversion answers every cast site -- (int),
 * intval(), settype() and the printf integer conversions all reach it -- so the
 * table below is the same value read six ways. PHL used to answer PHP_INT_MIN
 * for the whole column, in silence.
 *
 * The warning php prints beside each of these ("The float X is not
 * representable as an int, cast occurred") is suppressed here so the table
 * stands on its own; the diagnostic has its own test. */
$cases = [
    '1.9'        => 1.9,
    '-1.9'       => -1.9,
    'in range +' => 9223372036854774784.0,
    'in range -' => -9223372036854775808.0,
    '2**63'      => 9.2233720368547758E+18,
    '2**63+2048' => 9223372036854777856.0,
    '2**64'      => 18446744073709551616.0,
    '3e19'       => 3.0E+19,
    '1e19'       => 1.0E+19,
    '-1e19'      => -1.0E+19,
    '1e25'       => 1.0E+25,
    '1e30'       => 1.0E+30,
    '-1e30'      => -1.0E+30,
    '1e100'      => 1.0E+100,
    'max double' => 1.7976931348623157E+308,
    'NAN'        => NAN,
    'INF'        => INF,
    '-INF'       => -INF,
];
foreach ($cases as $label => $f) {
    $viaSettype = $f;
    @settype($viaSettype, 'int');
    printf("%-10s | %21d | %21d | %21s | %16s | %21s\n",
        $label, @(int)$f, @intval($f), $viaSettype,
        @sprintf('%x', $f), @sprintf('%u', $f));
}
/* The cast is one conversion, so the wrap is what a STRING OFFSET reads too --
 * php's own float offset, which warns about the cast and then indexes with the
 * wrapped value. (int)1e19 is negative, so that read is out of bounds; 2**64
 * wraps to 0 and reads the first byte. */
$s = 'abcdef';
var_dump(@$s[1.0E+19], @$s[18446744073709551616.0], @$s[2.9]);
/* An out-of-range float never becomes an int REPRESENTATION, so it stays a
 * float everywhere the engine caches one: identity, var_dump and the numeric
 * comparison are untouched by the cast rule above. */
$f = 1.0E+19;
var_dump($f, $f === 1.0E+19, $f > PHP_INT_MAX);
?>
--EXPECT--
1.9        |                     1 |                     1 |                     1 |                1 |                     1
-1.9       |                    -1 |                    -1 |                    -1 | ffffffffffffffff |  18446744073709551615
in range + |   9223372036854774784 |   9223372036854774784 |   9223372036854774784 | 7ffffffffffffc00 |   9223372036854774784
in range - |  -9223372036854775808 |  -9223372036854775808 |  -9223372036854775808 | 8000000000000000 |   9223372036854775808
2**63      |  -9223372036854775808 |  -9223372036854775808 |  -9223372036854775808 | 8000000000000000 |   9223372036854775808
2**63+2048 |  -9223372036854773760 |  -9223372036854773760 |  -9223372036854773760 | 8000000000000800 |   9223372036854777856
2**64      |                     0 |                     0 |                     0 |                0 |                     0
3e19       |  -6893488147419103232 |  -6893488147419103232 |  -6893488147419103232 | a055690d9db80000 |  11553255926290448384
1e19       |  -8446744073709551616 |  -8446744073709551616 |  -8446744073709551616 | 8ac7230489e80000 |  10000000000000000000
-1e19      |   8446744073709551616 |   8446744073709551616 |   8446744073709551616 | 7538dcfb76180000 |   8446744073709551616
1e25       |   1590897979265384448 |   1590897979265384448 |   1590897979265384448 | 1614014880000000 |   1590897979265384448
1e30       |   5076964154930102272 |   5076964154930102272 |   5076964154930102272 | 4675000000000000 |   5076964154930102272
-1e30      |  -5076964154930102272 |  -5076964154930102272 |  -5076964154930102272 | b98b000000000000 |  13369779918779449344
1e100      |                     0 |                     0 |                     0 |                0 |                     0
max double |                     0 |                     0 |                     0 |                0 |                     0
NAN        |                     0 |                     0 |                     0 |                0 |                     0
INF        |                     0 |                     0 |                     0 |                0 |                     0
-INF       |                     0 |                     0 |                     0 |                0 |                     0
string(0) ""
string(1) "a"
string(1) "c"
float(1.0E+19)
bool(true)
bool(true)
