--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff_assoc matches keys exactly and values through the user callback
--FILE--
<?php
// Keys take php's array-key identity (0 and "0" are the same key), values the callback.
$vcb = function ($a, $b) { return $a <=> $b; };
var_dump(array_udiff_assoc(["a" => 1, "b" => 2, "c" => 3], ["a" => "1"], ["c" => 4], $vcb));
var_dump(array_udiff_assoc([0 => "a", 1 => "b"], ["0" => "a"], "strcmp"));
var_dump(array_udiff_assoc(["k" => 7], $vcb));
?>
--EXPECT--
array(2) {
  ["b"]=>
  int(2)
  ["c"]=>
  int(3)
}
array(1) {
  [1]=>
  string(1) "b"
}
array(1) {
  ["k"]=>
  int(7)
}
