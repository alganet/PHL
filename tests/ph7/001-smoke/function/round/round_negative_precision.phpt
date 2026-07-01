--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: round() with negative precision rounds left of the decimal point
--FILE--
<?php
// Negative precision rounds to the left of the decimal point and always
// returns a float. These were silently wrong before (precision was clamped
// to 0). Uses var_export for byte-exact float comparison.
$cases = [
    [1234, -2], [1234, -3], [1250, -2], [1251, -2], [-1234, -2],
    [2.5, -1], [25, -1], [15, -1], [1234.5678, -2], [99, -2], [50, -2],
];
foreach ($cases as [$v, $p]) {
    printf("round(%s, %d)=%s\n", var_export($v, true), $p, var_export(round($v, $p), true));
}
// Negative precision with a non-default mode.
printf("round(25, -1, HALF_EVEN)=%s\n", var_export(round(25, -1, PHP_ROUND_HALF_EVEN), true));
printf("round(35, -1, HALF_EVEN)=%s\n", var_export(round(35, -1, PHP_ROUND_HALF_EVEN), true));
?>
--EXPECT--
round(1234, -2)=1200.0
round(1234, -3)=1000.0
round(1250, -2)=1300.0
round(1251, -2)=1300.0
round(-1234, -2)=-1200.0
round(2.5, -1)=0.0
round(25, -1)=30.0
round(15, -1)=20.0
round(1234.5678, -2)=1200.0
round(99, -2)=100.0
round(50, -2)=100.0
round(25, -1, HALF_EVEN)=20.0
round(35, -1, HALF_EVEN)=40.0
