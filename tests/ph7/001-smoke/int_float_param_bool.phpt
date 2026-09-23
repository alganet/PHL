--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A bool satisfies an int|float parameter, as weak mode says it does
--DESCRIPTION--
php's weak mode converts a bool to an `int|float` parameter — ceil(true) is
float(1) — and only refuses what no conversion produces. ceil(), floor() and
round() each carried a hand-rolled copy of that check, written before the shared
signature screen existed, and all three refused a bool outright: `ceil(true)`
threw where php answers float(1), and the neighbours reading the same declared
type (abs(), number_format()) accepted it, so the engine disagreed with itself.
ceil()'s copy had a second bug on top: one of its two branches printed no
", %s given" tail at all, so its TypeError ended mid-sentence. The three checks
are gone; the `int|float $num` row screens the argument, which also tightens the
string rule from SyStrIsNumeric to php's (a numeric PREFIX is not enough).
--FILE--
<?php
function ifbShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

$ifbObj = new stdClass;
$ifbRes = fopen('php://memory', 'r');
foreach (['ceil', 'floor', 'round', 'abs', 'number_format'] as $ifbFn) {
    ifbShow("$ifbFn true", fn() => $ifbFn(true));
    ifbShow("$ifbFn false", fn() => $ifbFn(false));
    ifbShow("$ifbFn int", fn() => $ifbFn(2));
    ifbShow("$ifbFn float", fn() => $ifbFn(4.2));
    ifbShow("$ifbFn numeric string", fn() => $ifbFn(' 4.7 '));
    ifbShow("$ifbFn prefix string", fn() => $ifbFn('4abc'));
    ifbShow("$ifbFn string", fn() => $ifbFn('abc'));
    ifbShow("$ifbFn array", fn() => $ifbFn([1]));
    ifbShow("$ifbFn object", fn() => $ifbFn($ifbObj));
    ifbShow("$ifbFn resource", fn() => $ifbFn($ifbRes));
}
--EXPECT--
ceil true => 1.0
ceil false => 0.0
ceil int => 2.0
ceil float => 5.0
ceil numeric string => 5.0
ceil prefix string => TypeError: ceil(): Argument #1 ($num) must be of type int|float, string given
ceil string => TypeError: ceil(): Argument #1 ($num) must be of type int|float, string given
ceil array => TypeError: ceil(): Argument #1 ($num) must be of type int|float, array given
ceil object => TypeError: ceil(): Argument #1 ($num) must be of type int|float, stdClass given
ceil resource => TypeError: ceil(): Argument #1 ($num) must be of type int|float, resource given
floor true => 1.0
floor false => 0.0
floor int => 2.0
floor float => 4.0
floor numeric string => 4.0
floor prefix string => TypeError: floor(): Argument #1 ($num) must be of type int|float, string given
floor string => TypeError: floor(): Argument #1 ($num) must be of type int|float, string given
floor array => TypeError: floor(): Argument #1 ($num) must be of type int|float, array given
floor object => TypeError: floor(): Argument #1 ($num) must be of type int|float, stdClass given
floor resource => TypeError: floor(): Argument #1 ($num) must be of type int|float, resource given
round true => 1.0
round false => 0.0
round int => 2.0
round float => 4.0
round numeric string => 5.0
round prefix string => TypeError: round(): Argument #1 ($num) must be of type int|float, string given
round string => TypeError: round(): Argument #1 ($num) must be of type int|float, string given
round array => TypeError: round(): Argument #1 ($num) must be of type int|float, array given
round object => TypeError: round(): Argument #1 ($num) must be of type int|float, stdClass given
round resource => TypeError: round(): Argument #1 ($num) must be of type int|float, resource given
abs true => 1
abs false => 0
abs int => 2
abs float => 4.2
abs numeric string => 4.7
abs prefix string => TypeError: abs(): Argument #1 ($num) must be of type int|float, string given
abs string => TypeError: abs(): Argument #1 ($num) must be of type int|float, string given
abs array => TypeError: abs(): Argument #1 ($num) must be of type int|float, array given
abs object => TypeError: abs(): Argument #1 ($num) must be of type int|float, stdClass given
abs resource => TypeError: abs(): Argument #1 ($num) must be of type int|float, resource given
number_format true => '1'
number_format false => '0'
number_format int => '2'
number_format float => '4'
number_format numeric string => '5'
number_format prefix string => TypeError: number_format(): Argument #1 ($num) must be of type int|float, string given
number_format string => TypeError: number_format(): Argument #1 ($num) must be of type int|float, string given
number_format array => TypeError: number_format(): Argument #1 ($num) must be of type int|float, array given
number_format object => TypeError: number_format(): Argument #1 ($num) must be of type int|float, stdClass given
number_format resource => TypeError: number_format(): Argument #1 ($num) must be of type int|float, resource given
