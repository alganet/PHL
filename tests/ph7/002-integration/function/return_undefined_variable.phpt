--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
return of a lone undefined variable warns at the read (like echo/interpolation)
--DESCRIPTION--
php warns "Undefined variable" when a `return` READS an undefined variable — the value is
consumed. PHL used to load a lone-variable return operand quietly (the same quiet load that
correctly keeps a bare `$z;` expression statement silent, which php also leaves silent), so
`return $z` returned NULL without a warning while `echo $z` / `$x=$z` / `$z+0` all warned.
Compiling the `return` operand read-only (EXPR_FLAG_RDONLY_LOAD, the flag echo and string
interpolation already use) makes the lone-variable case warn php-exact and returns NULL; a
compound operand (`$a+$b`) already warned via the arithmetic load. A bare `$z;` statement and a
defined variable stay silent; the generator `return` path shares the same compiled operand. The
handler normalizes the warning prefix/stream across engines.
--FILE--
<?php
set_error_handler(function ($no, $str, $file, $line) { echo "[$no L$line] $str\n"; return true; });

echo "== return of a lone undefined variable warns, returns null ==\n";
function f1() { return $ruv_missing; }
var_dump(f1());

echo "== return of a defined variable is silent ==\n";
function f2() { $ruv_have = 7; return $ruv_have; }
var_dump(f2());

echo "== return of a compound expression warns per operand ==\n";
function f3() { return $ruv_a + $ruv_b; }
var_dump(f3());

echo "== bare expression statement stays silent (matches php) ==\n";
function f4() { $ruv_bare; return 1; }
var_dump(f4());

echo "== return of a defined array is intact ==\n";
function f5() { return [1, 2]; }
var_dump(f5());

echo "== generator return of an undefined variable warns ==\n";
function g1() { return $ruv_gen; yield 1; }
$g = g1();
foreach ($g as $v) {}
var_dump($g->getReturn());
?>
--EXPECT--
== return of a lone undefined variable warns, returns null ==
[2 L5] Undefined variable $ruv_missing
NULL
== return of a defined variable is silent ==
int(7)
== return of a compound expression warns per operand ==
[2 L13] Undefined variable $ruv_a
[2 L13] Undefined variable $ruv_b
int(0)
== bare expression statement stays silent (matches php) ==
int(1)
== return of a defined array is intact ==
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
== generator return of an undefined variable warns ==
[2 L25] Undefined variable $ruv_gen
NULL
