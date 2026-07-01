--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: round() honors the HALF_* rounding modes (PHP_ROUND_HALF_UP/DOWN/EVEN/ODD)
--FILE--
<?php
// HALF_* mode matrix at precision 0. Uses var_export so floats compare
// byte-exact across engines (echo/print would truncate to the display
// precision). Clean .5 values (no float error) so these are stable across
// all PHP versions; the 8.5-only integer modes 5..8 live in
// round_modes_int.phpt (which is gated to PHP 8.5+).
$vals = [2.5, 3.5, -2.5, -3.5, 0.5, 1.5];
$modes = [
    'HALF_UP'   => PHP_ROUND_HALF_UP,
    'HALF_DOWN' => PHP_ROUND_HALF_DOWN,
    'HALF_EVEN' => PHP_ROUND_HALF_EVEN,
    'HALF_ODD'  => PHP_ROUND_HALF_ODD,
];
foreach ($modes as $name => $m) {
    foreach ($vals as $v) {
        printf("%s round(%s)=%s\n", $name, var_export($v, true), var_export(round($v, 0, $m), true));
    }
}
?>
--EXPECT--
HALF_UP round(2.5)=3.0
HALF_UP round(3.5)=4.0
HALF_UP round(-2.5)=-3.0
HALF_UP round(-3.5)=-4.0
HALF_UP round(0.5)=1.0
HALF_UP round(1.5)=2.0
HALF_DOWN round(2.5)=2.0
HALF_DOWN round(3.5)=3.0
HALF_DOWN round(-2.5)=-2.0
HALF_DOWN round(-3.5)=-3.0
HALF_DOWN round(0.5)=0.0
HALF_DOWN round(1.5)=1.0
HALF_EVEN round(2.5)=2.0
HALF_EVEN round(3.5)=4.0
HALF_EVEN round(-2.5)=-2.0
HALF_EVEN round(-3.5)=-4.0
HALF_EVEN round(0.5)=0.0
HALF_EVEN round(1.5)=2.0
HALF_ODD round(2.5)=3.0
HALF_ODD round(3.5)=3.0
HALF_ODD round(-2.5)=-3.0
HALF_ODD round(-3.5)=-3.0
HALF_ODD round(0.5)=1.0
HALF_ODD round(1.5)=1.0
