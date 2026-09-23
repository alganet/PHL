--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_ukey compares keys through the user callback and ignores values
--FILE--
<?php
// Keys through the callback, values never consulted; int and string keys both
// reach the callback as-is.
$kcb = function ($a, $b) { return strcasecmp((string)$a, (string)$b); };
var_dump(array_diff_ukey(["a" => 1, "b" => 2, "c" => 3], ["A" => 9], ["b" => 8], $kcb));
var_dump(array_diff_ukey([0 => "x", 1 => "y", "5" => "z"], [1 => "q"], function ($a, $b) { return $a <=> $b; }));
// No comparand array: php answers the first array as-is.
var_dump(array_diff_ukey(["k" => 7], $kcb));
?>
--EXPECT--
array(1) {
  ["c"]=>
  int(3)
}
array(2) {
  [0]=>
  string(1) "x"
  [5]=>
  string(1) "z"
}
array(1) {
  ["k"]=>
  int(7)
}
