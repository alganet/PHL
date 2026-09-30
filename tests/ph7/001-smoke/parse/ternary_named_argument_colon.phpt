--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument's colon is not the enclosing ternary's
--DESCRIPTION--
Regression for ExprVerifyNodes: colons were counted against a bare tally of open
'?'s, so the LABEL colon of a named argument spent the ternary's question mark
and the real ':' arrived with nothing open --
`Syntax error: Unexpected token ':'`. Only the unparenthesized form showed it:
`$r = ($c ? f(b: 2) : 'n');` and `var_dump($c ? f(b: 2) : 'n')` both parsed,
because an enclosing '(' put the '?' and both colons at depths that worked out.
A ternary's ':' is the one standing at the same nesting depth as its '?'.
Real source: nette/utils' Type::fromReflection() and pest's Mixins/Expectation.php.
--FILE--
<?php
function tnac_f($a = 1, $b = 2) { return "$a-$b"; }
$tnac_c = true;

var_dump($tnac_c ? tnac_f(b: 9) : 'n');
var_dump(!$tnac_c ? tnac_f(b: 9) : 'n');
$tnac_r = $tnac_c ? tnac_f(b: 8) : 'n';
var_dump($tnac_r);
var_dump($tnac_c ? tnac_f(b: 7) : tnac_f(b: 6));
var_dump(tnac_f(b: $tnac_c ? 5 : 4));
var_dump($tnac_c ? tnac_f(b: 1) : ($tnac_c ? tnac_f(b: 2) : 'n'));

// The shapes that already worked must keep working.
var_dump(($tnac_c ? tnac_f(b: 3) : 'n'));
var_dump($tnac_c ? 1 : 2);
var_dump($tnac_c ?: 'e');
var_dump($tnac_c ? ($tnac_c ? 'a' : 'b') : 'c');
var_dump(true ? 'x' : (false ? 'y' : 'z'));

// A subscript's brackets nest the same way a call's parens do.
$tnac_a = ['x', 'y', 'z'];
var_dump($tnac_a[$tnac_c ? 1 : 2]);
var_dump($tnac_a[$tnac_c ? tnac_f(b: 0) : 0] ?? 'none');
?>
--EXPECT--
string(3) "1-9"
string(1) "n"
string(3) "1-8"
string(3) "1-7"
string(3) "1-5"
string(3) "1-1"
string(3) "1-3"
int(1)
bool(true)
string(1) "a"
string(1) "x"
string(1) "y"
string(4) "none"
--CLEAN--
<?php
unset($tnac_c, $tnac_r, $tnac_a);
