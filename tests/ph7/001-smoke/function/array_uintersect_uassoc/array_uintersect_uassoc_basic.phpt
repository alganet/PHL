--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect_uassoc matches keys and values through their own callbacks
--FILE--
<?php
$vcb = function ($a, $b) { return $a <=> $b; };
$kcb = function ($a, $b) { return strcasecmp((string)$a, (string)$b); };
var_dump(array_uintersect_uassoc(["a" => 1, "b" => 2], ["A" => "1", "B" => 2], ["a" => 1, "b" => 2], $vcb, $kcb));
var_dump(array_uintersect_uassoc(["k" => 7], $vcb, $kcb));
?>
--EXPECT--
array(2) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
}
array(1) {
  ["k"]=>
  int(7)
}
