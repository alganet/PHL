--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func() hands a by-reference BUILTIN parameter a copy, like php
--FILE--
<?php
error_reporting(E_ALL);
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

echo "== the caller's array is NOT sorted ==\n";
$a = [3, 1, 2];
var_dump(call_user_func('sort', $a));
var_dump($a);

echo "== the caller's array does NOT lose its last element ==\n";
$b = [3, 1, 2];
var_dump(call_user_func('array_pop', $b));
var_dump($b);

echo "== the caller's \$matches variable is NOT filled ==\n";
$m = 'untouched';
var_dump(call_user_func('preg_match', '/(a)/', 'a', $m));
var_dump($m);

echo "== a by-VALUE builtin parameter is unaffected ==\n";
var_dump(call_user_func('strtoupper', 'ab'));

echo "== a DIRECT call still binds and writes back ==\n";
$c = [3, 1, 2];
sort($c);
var_dump($c);
$m2 = 'untouched';
preg_match('/(a)/', 'a', $m2);
var_dump($m2[1]);
?>
--EXPECT--
== the caller's array is NOT sorted ==
  [2] sort(): Argument #1 ($array) must be passed by reference, value given
bool(true)
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
== the caller's array does NOT lose its last element ==
  [2] array_pop(): Argument #1 ($array) must be passed by reference, value given
int(2)
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
== the caller's $matches variable is NOT filled ==
  [2] preg_match(): Argument #3 ($matches) must be passed by reference, value given
int(1)
string(9) "untouched"
== a by-VALUE builtin parameter is unaffected ==
string(2) "AB"
== a DIRECT call still binds and writes back ==
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
string(1) "a"
