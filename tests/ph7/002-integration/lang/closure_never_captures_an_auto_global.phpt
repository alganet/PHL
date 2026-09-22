--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure never captures an auto-global: $GLOBALS stays the LIVE symbol table
--DESCRIPTION--
php's auto-globals are visible in every scope without importing them, and it refuses to let one
be captured -- `use ($GLOBALS)` is a compile fatal, and an arrow function does not auto-capture
one. In PHL that refusal is load-bearing, not cosmetic: a capture resolves the name through
VmExtractMemObj, which consults the superglobal table FIRST, so installing the captured value
wrote over the superglobal's own slot. CALLING `fn() => $GLOBALS['a']` therefore replaced the
live symbol-table view with the by-value SNAPSHOT taken when the closure was created, and every
global defined after that point became invisible -- to the closure, to other functions, and to
top-level code. `$_SERVER` and friends had the same exposure. extract() already screened the
same table for the same reason (VmExtractIsProtected).

`$argv` is deliberately NOT screened: PHL installs it as a superglobal for convenience, but
php's is an ordinary global, so capturing it is valid php and stays legal here.
--FILE--
<?php
echo "== the live view survives calling a closure that reads it\n";
$a = 1;
$f = fn() => $GLOBALS['a'] ?? 'MISS';
echo $f(), "\n";

$b = 2;
echo $GLOBALS['b'] ?? 'MISS-AT-TOP', "\n";

$g = fn() => $GLOBALS['b'] ?? 'MISS-IN-ARROW';
echo $g(), "\n";

function readsGlobal() { return $GLOBALS['b'] ?? 'MISS-IN-FUNCTION'; }
echo readsGlobal(), "\n";

$c = 3;
echo $GLOBALS['c'] ?? 'MISS-C', "\n";
echo $g(), "\n";

echo "== a plain closure body is the same\n";
$h = function () { return $GLOBALS['c'] ?? 'MISS-IN-CLOSURE'; };
echo $h(), "\n";
$d = 4;
echo $GLOBALS['d'] ?? 'MISS-D', "\n";

echo "== \$_SERVER stays live too\n";
$_SERVER['PHL_ONE'] = 'one';
$s = fn() => $_SERVER['PHL_ONE'] ?? 'MISS-ONE';
echo $s(), "\n";
$_SERVER['PHL_TWO'] = 'two';
echo $_SERVER['PHL_TWO'] ?? 'MISS-TWO', "\n";
echo $s(), "\n";

echo "== \$argv is an ordinary global and stays capturable\n";
$u = function () use ($argv) { return is_array($argv); };
var_dump($u());
$v = fn() => is_array($argv);
var_dump($v());

echo "== ordinary captures are untouched\n";
$x = 10;
$k = fn() => $x;
$x = 20;
echo $k(), "\n";
$w = function () use ($x) { return $x; };
$x = 30;
echo $w(), "\n";
?>
--EXPECT--
== the live view survives calling a closure that reads it
1
2
2
2
3
2
== a plain closure body is the same
3
4
== $_SERVER stays live too
one
two
one
== $argv is an ordinary global and stays capturable
bool(true)
bool(true)
== ordinary captures are untouched
10
20
