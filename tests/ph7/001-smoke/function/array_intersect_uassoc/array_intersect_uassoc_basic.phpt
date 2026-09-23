--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_uassoc matches keys by callback and values as strings
--FILE--
<?php
// Keys through the callback, values through php's own (string)$a === (string)$b.
$kcb = function ($a, $b) { return strcasecmp((string)$a, (string)$b); };
var_dump(array_intersect_uassoc(["a" => "green", "b" => "brown", "c" => "blue"], ["A" => "GREEN", "B" => "brown"], $kcb));
var_dump(array_intersect_uassoc(["a" => "1"], ["A" => 1], $kcb));
var_dump(array_intersect_uassoc(["k" => 7], $kcb));
?>
--EXPECT--
array(1) {
  ["b"]=>
  string(5) "brown"
}
array(1) {
  ["a"]=>
  string(1) "1"
}
array(1) {
  ["k"]=>
  int(7)
}
