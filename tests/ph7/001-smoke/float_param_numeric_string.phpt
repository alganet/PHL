--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A float parameter refuses a non-numeric string, as an int parameter does
--DESCRIPTION--
php's weak mode coerces a NUMERIC string to a `float` parameter and refuses every
other one, exactly as it does for `int`. The shared signature screen only knew the
int half — PH7_IntArgResolve gave every builtin its own int check and there was no
float twin — so a `float $num` builtin that did not hand-roll one converted the
string to 0.0 and COMPUTED with it: cos('nope') answered float(1), sqrt('nope')
float(0), log('nope') float(-INF). Numbers with nothing wrong-looking about them,
from input php refuses outright. Nine more never reached the screen at all
because they had no signature ROW to screen against — the eight `float $num`
routines acosh/asinh/atanh/deg2rad/expm1/log1p/rad2deg and php 8.5's fpow(). fdiv()
was the same hazard one layer down: it is written in the prelude and its two
parameters carried no type at all, so fdiv('abc', 2) answered float(0).
--FILE--
<?php
function fpnShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

/* The C builtins that computed on a 0.0 nobody supplied. */
fpnShow('cos', fn() => cos('nope'));
fpnShow('sqrt', fn() => sqrt('nope'));
fpnShow('log', fn() => log('nope'));
fpnShow('exp', fn() => exp('nope'));
fpnShow('fmod', fn() => fmod('nope', 1.0));
fpnShow('hypot second', fn() => hypot(1.0, 'nope'));

/* A numeric PREFIX is not enough, and neither is a hex literal — the same rule
 * the int arm follows. */
fpnShow('leading-numeric', fn() => sqrt('4abc'));
fpnShow('hex', fn() => sqrt('0x4'));
fpnShow('empty', fn() => sqrt(''));

/* The ones that had no signature row at all, so nothing described them to the
 * screen: each computed on a 0.0 or on the length of the word "Array". */
foreach (['acosh', 'asinh', 'atanh', 'deg2rad', 'expm1', 'log1p', 'rad2deg'] as $fpnFn) {
    fpnShow($fpnFn, fn() => $fpnFn('nope'));
    fpnShow("$fpnFn array", fn() => $fpnFn([1]));
}
fpnShow('fpow', fn() => fpow('nope', 2));
fpnShow('fpow ok', fn() => fpow(2, 3));
fpnShow('rad2deg ok', fn() => rad2deg(M_PI));

/* fdiv() is written in the prelude; its float parameters are php's own. */
fpnShow('fdiv non-numeric', fn() => fdiv('abc', 2));
fpnShow('fdiv array', fn() => fdiv([1], 2));
fpnShow('fdiv second', fn() => fdiv(1, 'abc'));

/* Weak mode is not tightened: a numeric string still coerces, in every shape
 * php accepts (leading and trailing whitespace included), and so do int, float
 * and bool. */
fpnShow('numeric string', fn() => sqrt('16'));
fpnShow('float string', fn() => sqrt('2.25'));
fpnShow('exponent string', fn() => sqrt('1.6e1'));
fpnShow('padded string', fn() => sqrt(" 9\t"));
fpnShow('int', fn() => sqrt(25));
fpnShow('bool', fn() => sqrt(true));
fpnShow('fdiv numeric string', fn() => fdiv('3', '4'));
fpnShow('fdiv by zero', fn() => fdiv(1, 0));

/* An arm the string CAN satisfy keeps the parameter unscreened, and the
 * `int|float` union words its refusal with both names. */
fpnShow('union int|float', fn() => round('nope'));
fpnShow('union with string', fn() => number_format(1234.5, 2, '.', ','));
fpnShow('mixed parameter', fn() => max('nope', 2));

/* Reflection prints what the declaration says. */
$fpnRef = new ReflectionFunction('fdiv');
echo 'fdiv sig => ';
foreach ($fpnRef->getParameters() as $fpnParam) {
    echo (string) $fpnParam->getType(), ' $', $fpnParam->getName(), ' ';
}
echo ': ', (string) $fpnRef->getReturnType(), "\n";
--EXPECT--
cos => TypeError: cos(): Argument #1 ($num) must be of type float, string given
sqrt => TypeError: sqrt(): Argument #1 ($num) must be of type float, string given
log => TypeError: log(): Argument #1 ($num) must be of type float, string given
exp => TypeError: exp(): Argument #1 ($num) must be of type float, string given
fmod => TypeError: fmod(): Argument #1 ($num1) must be of type float, string given
hypot second => TypeError: hypot(): Argument #2 ($y) must be of type float, string given
leading-numeric => TypeError: sqrt(): Argument #1 ($num) must be of type float, string given
hex => TypeError: sqrt(): Argument #1 ($num) must be of type float, string given
empty => TypeError: sqrt(): Argument #1 ($num) must be of type float, string given
acosh => TypeError: acosh(): Argument #1 ($num) must be of type float, string given
acosh array => TypeError: acosh(): Argument #1 ($num) must be of type float, array given
asinh => TypeError: asinh(): Argument #1 ($num) must be of type float, string given
asinh array => TypeError: asinh(): Argument #1 ($num) must be of type float, array given
atanh => TypeError: atanh(): Argument #1 ($num) must be of type float, string given
atanh array => TypeError: atanh(): Argument #1 ($num) must be of type float, array given
deg2rad => TypeError: deg2rad(): Argument #1 ($num) must be of type float, string given
deg2rad array => TypeError: deg2rad(): Argument #1 ($num) must be of type float, array given
expm1 => TypeError: expm1(): Argument #1 ($num) must be of type float, string given
expm1 array => TypeError: expm1(): Argument #1 ($num) must be of type float, array given
log1p => TypeError: log1p(): Argument #1 ($num) must be of type float, string given
log1p array => TypeError: log1p(): Argument #1 ($num) must be of type float, array given
rad2deg => TypeError: rad2deg(): Argument #1 ($num) must be of type float, string given
rad2deg array => TypeError: rad2deg(): Argument #1 ($num) must be of type float, array given
fpow => TypeError: fpow(): Argument #1 ($num) must be of type float, string given
fpow ok => 8.0
rad2deg ok => 180.0
fdiv non-numeric => TypeError: fdiv(): Argument #1 ($num1) must be of type float, string given
fdiv array => TypeError: fdiv(): Argument #1 ($num1) must be of type float, array given
fdiv second => TypeError: fdiv(): Argument #2 ($num2) must be of type float, string given
numeric string => 4.0
float string => 1.5
exponent string => 4.0
padded string => 3.0
int => 5.0
bool => 1.0
fdiv numeric string => 0.75
fdiv by zero => INF
union int|float => TypeError: round(): Argument #1 ($num) must be of type int|float, string given
union with string => '1,234.50'
mixed parameter => 'nope'
fdiv sig => float $num1 float $num2 : float
