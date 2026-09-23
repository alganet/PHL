--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_intersect_ukey keeps entries whose key every other array matches by callback
--FILE--
<?php
// Kept only when EVERY other array has a callback-equal key.
$kcb = function ($a, $b) { return strcasecmp((string)$a, (string)$b); };
var_dump(array_intersect_ukey(["a" => 1, "b" => 2, "c" => 3], ["A" => 9, "B" => 8], ["b" => 7, "C" => 6], $kcb));
var_dump(array_intersect_ukey([10 => "a", "x" => "b"], ["10" => "q", "X" => "r"], $kcb));
var_dump(array_intersect_ukey(["k" => 7], $kcb));
?>
--EXPECT--
array(1) {
  ["b"]=>
  int(2)
}
array(2) {
  [10]=>
  string(1) "a"
  ["x"]=>
  string(1) "b"
}
array(1) {
  ["k"]=>
  int(7)
}
