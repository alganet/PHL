--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A comparison callback's 64-bit return is reduced by sign, not truncated
--FILE--
<?php
// A comparator's verdict is its SIGN on the full 64 bits: 1<<32 is "unequal",
// not the 0 a 32-bit truncation reads.
var_dump(array_udiff([7], [7], function ($a, $b) { return 4294967296; }));
var_dump(array_udiff_assoc(["k" => 7], ["k" => 7], function ($a, $b) { return -4294967296; }));
var_dump(array_uintersect([7], [7], function ($a, $b) { return 4294967296; }));
?>
--EXPECT--
array(1) {
  [0]=>
  int(7)
}
array(1) {
  ["k"]=>
  int(7)
}
array(0) {
}
