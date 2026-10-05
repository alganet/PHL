--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func() hands every callable spelling's by-reference parameter a copy, with php's warning
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

class D {
    public function mk() { return function (&$x) { $x++; echo "  closure saw $x\n"; }; }
    public static function s(&$x) { $x++; echo "  static saw $x\n"; }
    public function i(&$x) { $x++; echo "  method saw $x\n"; }
}
function v($a, &...$rest) { echo "  variadic got ", count($rest), "\n"; }
function n($a, &$b = 0, &$c = 0) { $c = 5; }

$callables = [
    'closure in a method' => (new D)->mk(),
    'top-level closure'   => function (&$x) { $x++; echo "  closure saw $x\n"; },
    'arrow function'      => fn (&$x) => $x = 99,
    "['D', 's']"          => ['D', 's'],
    '[$obj, \'i\']'       => [new D, 'i'],
    "'D::s'"              => 'D::s',
    'D::s(...)'           => D::s(...),
];
foreach ($callables as $label => $cb) {
    echo "== $label ==\n";
    $v = 1;
    call_user_func($cb, 1);          // a literal: php warns and runs, never refuses
    call_user_func($cb, $v);         // a variable: the callee gets a COPY
    var_dump($v);
}

echo "== a by-ref variadic tail warns per element, unnamed ==\n";
call_user_func('v', 1, 2, 3);

echo "== a name: argument is reported by the formal it picks ==\n";
$x = 1;
call_user_func('n', 1, c: $x);
var_dump($x);

echo "== a first-class callable over a builtin copies too ==\n";
$a = [3, 1, 2];
call_user_func(sort(...), $a);
call_user_func_array(sort(...), [$a]);
var_dump($a);

echo "== __call routing has no by-ref formal to warn about ==\n";
class M { public function __call($n, $a) { echo "  __call $n\n"; } private function p(&$x) {} }
call_user_func([new M, 'p'], $x);

echo "== a handler throwing on the warning stops before the callee ==\n";
set_error_handler(function ($no, $str) { throw new Exception($str); });
try {
    call_user_func(function (&$q) { echo "  ENTERED\n"; }, $x);
} catch (Exception $e) {
    echo "  caught: ", $e->getMessage(), "\n";
}
--EXPECTF--
== closure in a method ==
  [2] D::{closure:D::mk():6}(): Argument #1 ($x) must be passed by reference, value given
  closure saw 2
  [2] D::{closure:D::mk():6}(): Argument #1 ($x) must be passed by reference, value given
  closure saw 2
int(1)
== top-level closure ==
  [2] {closure:%s:15}(): Argument #1 ($x) must be passed by reference, value given
  closure saw 2
  [2] {closure:%s:15}(): Argument #1 ($x) must be passed by reference, value given
  closure saw 2
int(1)
== arrow function ==
  [2] {closure:%s:16}(): Argument #1 ($x) must be passed by reference, value given
  [2] {closure:%s:16}(): Argument #1 ($x) must be passed by reference, value given
int(1)
== ['D', 's'] ==
  [2] D::s(): Argument #1 ($x) must be passed by reference, value given
  static saw 2
  [2] D::s(): Argument #1 ($x) must be passed by reference, value given
  static saw 2
int(1)
== [$obj, 'i'] ==
  [2] D::i(): Argument #1 ($x) must be passed by reference, value given
  method saw 2
  [2] D::i(): Argument #1 ($x) must be passed by reference, value given
  method saw 2
int(1)
== 'D::s' ==
  [2] D::s(): Argument #1 ($x) must be passed by reference, value given
  static saw 2
  [2] D::s(): Argument #1 ($x) must be passed by reference, value given
  static saw 2
int(1)
== D::s(...) ==
  [2] D::s(): Argument #1 ($x) must be passed by reference, value given
  static saw 2
  [2] D::s(): Argument #1 ($x) must be passed by reference, value given
  static saw 2
int(1)
== a by-ref variadic tail warns per element, unnamed ==
  [2] v(): Argument #2 must be passed by reference, value given
  [2] v(): Argument #3 must be passed by reference, value given
  variadic got 2
== a name: argument is reported by the formal it picks ==
  [2] n(): Argument #3 ($c) must be passed by reference, value given
int(1)
== a first-class callable over a builtin copies too ==
  [2] sort(): Argument #1 ($array) must be passed by reference, value given
  [2] sort(): Argument #1 ($array) must be passed by reference, value given
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
== __call routing has no by-ref formal to warn about ==
  __call p
== a handler throwing on the warning stops before the callee ==
  caught: {closure:%s:51}(): Argument #1 ($q) must be passed by reference, value given
