--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_multisort is stable and keeps a passenger column's string keys
--FILE--
<?php
// The sort is stable: rows tying on every column keep their original order
// (visible through a passenger column).
$c1 = [1, 1, 1, 0];
$c2 = ["a", "b", "c", "d"];
array_multisort($c1, $c2);
var_dump($c2);
// natural + case-insensitive flags
$n = ["img12", "img10", "IMG2", "img1"];
array_multisort($n, SORT_NATURAL | SORT_FLAG_CASE);
var_dump($n);
// a passenger column's string keys are preserved through the permutation
$k1 = [2, 1];
$k2 = ["x" => "a", "y" => "b"];
array_multisort($k1, $k2);
var_dump($k2);
// empty columns are fine
$e1 = []; $e2 = [];
var_dump(array_multisort($e1, $e2));
?>
--EXPECT--
array(4) {
  [0]=>
  string(1) "d"
  [1]=>
  string(1) "a"
  [2]=>
  string(1) "b"
  [3]=>
  string(1) "c"
}
array(4) {
  [0]=>
  string(4) "img1"
  [1]=>
  string(4) "IMG2"
  [2]=>
  string(5) "img10"
  [3]=>
  string(5) "img12"
}
array(2) {
  ["y"]=>
  string(1) "b"
  ["x"]=>
  string(1) "a"
}
bool(true)
