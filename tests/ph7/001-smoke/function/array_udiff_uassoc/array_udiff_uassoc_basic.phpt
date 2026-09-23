--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff_uassoc takes a value comparator then a key comparator
--FILE--
<?php
// Two callbacks: the VALUE comparator is the second-to-last argument, the KEY
// comparator the last.
$vcb = function ($a, $b) { return $a <=> $b; };
$kcb = function ($a, $b) { return strcasecmp((string)$a, (string)$b); };
var_dump(array_udiff_uassoc(["a" => 1, "b" => 2], ["A" => "1"], ["B" => 2], $vcb, $kcb));
var_dump(array_udiff_uassoc(["a" => 1], [], [], $vcb, $kcb));
var_dump(array_udiff_uassoc(["k" => 7], $vcb, $kcb));
?>
--EXPECT--
array(0) {
}
array(1) {
  ["a"]=>
  int(1)
}
array(1) {
  ["k"]=>
  int(7)
}
