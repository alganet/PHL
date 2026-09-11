--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A callable string with a leading backslash resolves to the global function
--FILE--
<?php
var_dump(array_map('\trim', [' a ', ' b ']));
var_dump(call_user_func('\strlen', 'hello'));
var_dump(is_callable('\trim'));
var_dump(function_exists('\strtoupper'));
$f = '\strtoupper';
var_dump($f('hi'));
var_dump(array_map('\intval', ['7', '8']));
?>
--EXPECT--
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
}
int(5)
bool(true)
bool(true)
string(2) "HI"
array(2) {
  [0]=>
  int(7)
  [1]=>
  int(8)
}
--CLEAN--
<?php
