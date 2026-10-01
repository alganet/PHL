--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: an arrow nested in an arrow's body may declare a return type
--FILE--
<?php
// The outer body's scan once took the inner ':' for an enclosing ternary's colon
// and ended the body there, so every one of these was a parse error.
$f = fn(int $t) => fn($v): string => "a" . $v;
var_dump($f(1)(2));
$f = fn($t) => fn($v): ?string => $v;
var_dump($f(1)(null));
$f = fn($t) => fn($v): int|string => $v;
var_dump($f(1)("x"));
$f = fn($t) => static fn($v): string => $v;
var_dump($f(1)("s"));
$f = fn($t) => fn($v): string => $v ? 'a' : 'b';
var_dump($f(1)(0), $f(1)(1));
$f = fn($t) => fn($v): ?\Countable => $v;
var_dump($f(1)(null));
$f = fn($t) => fn($v): iterable => $v;
var_dump($f(1)([]));
$f = fn($t) => fn($v): array => $v;
var_dump($f(1)([1]));
$f = fn($t) => $t ? fn($v): string => 'y' : fn($v): string => 'n';
var_dump($f(1)('x'), $f(0)('x'));
$f = fn($t) => [fn($v): string => $v, 'k' => fn($v): int => $v];
var_dump($f(1)['k'](4));
$f = fn($t): callable => fn($v): callable => fn($w): string => "$t$v$w";
var_dump($f(1)(2)(3));
class A {
    public function m() { return fn($t) => fn($v): static => $this; }
}
var_dump((new A)->m()(1)(2) instanceof A);
$g = fn($t) => fn($v): (\Countable&\ArrayAccess)|null => $v;
var_dump($g(1)(null));
?>
--EXPECT--
string(2) "a2"
NULL
string(1) "x"
string(1) "s"
string(1) "b"
string(1) "a"
NULL
array(0) {
}
array(1) {
  [0]=>
  int(1)
}
string(1) "y"
string(1) "n"
int(4)
string(3) "123"
bool(true)
NULL
