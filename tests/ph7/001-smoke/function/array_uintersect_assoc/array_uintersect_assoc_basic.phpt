--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect_assoc keeps entries whose key and callback-equal value every array shares
--FILE--
<?php
// Kept only when every other array holds the SAME key with a callback-equal value.
$vcb = function ($a, $b) { return $a <=> $b; };
var_dump(array_uintersect_assoc(["a" => 1, "b" => 2], ["a" => "1", "b" => 5], ["A" => 1, "b" => "2"], $vcb));
var_dump(array_uintersect_assoc(["z" => 3, "a" => 1, "m" => 2], ["a" => 1, "m" => 2, "z" => 3], $vcb));
var_dump(array_uintersect_assoc(["k" => 7], $vcb));
?>
--EXPECT--
array(0) {
}
array(3) {
  ["z"]=>
  int(3)
  ["a"]=>
  int(1)
  ["m"]=>
  int(2)
}
array(1) {
  ["k"]=>
  int(7)
}
