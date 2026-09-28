--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A float literal may start with the dot, and the scanner takes it greedily
--DESCRIPTION--
php's DNUM is `({LNUM}?"."{LNUM})|({LNUM}"."{LNUM}?)`, so both halves are
optional and `.5` is a float literal -- the everyday way a fraction below one is
written. PHL had no such token: the dot was always the concatenation operator, so
`$a = .5;` was `syntax error, unexpected token ";"` on source php runs.

The rule is the SCANNER's and it is unconditional, which is the second half of
this test: php reads `"x".5` as a string followed by the float `.5` and reports a
parse error for it, so a program that means concatenation has to write the space.
The exponent rides the same longest-match rule -- `.5e3` is one token, `.5e+` is
the float `.5` followed by the identifier `e`, because no digit follows the sign.
--FILE--
<?php
var_dump(.5, .0, .25 + .25, -.5, .5e3, .5E-3, .1_1);
var_dump([.5][0], (int) .9);

function dotHalf($x = .5) { return $x; }
var_dump(dotHalf(), dotHalf(.75));

const DOT_CONST = .5;
var_dump(DOT_CONST);

/* Concatenation still needs its space, and a dot with no digit after it is
 * still the operator. */
$dotLeft = 'x';
var_dump($dotLeft . 5, $dotLeft . '5', 1 . 5);
?>
--EXPECT--
float(0.5)
float(0)
float(0.5)
float(-0.5)
float(500)
float(0.0005)
float(0.11)
float(0.5)
int(0)
float(0.5)
float(0.75)
float(0.5)
string(2) "x5"
string(2) "x5"
string(2) "15"
