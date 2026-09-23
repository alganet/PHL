--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
RECORDED DIVERGENCE (degenerate comparator input only): a comparand array holding two keys its own callback equates. PHL scans every callback-equal candidate, so an entry is dropped by diff (kept by intersect) when ANY candidate also matches its value; php's sort-merge consults the one candidate its zend_sort tie order lines up, an order its documentation leaves undefined. The _zend twin pins php's answers. Real comparators over real data never present two equal keys in one array.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
// A comparand array holding TWO keys the callback itself equates ("K" and "k"
// under strcasecmp). Which candidate's VALUE gets compared is php's zend_sort
// tie order — an artifact its documentation leaves undefined.
$icase = fn($a, $b) => strcasecmp((string)$a, (string)$b);
$vcb = fn($a, $b) => $a <=> $b;
var_dump(array_diff_uassoc(["k" => 5], ["K" => 1, "k" => 5], $icase));
var_dump(array_intersect_uassoc(["k" => 5], ["K" => 1, "k" => 5], $icase));
var_dump(array_udiff_uassoc(["k" => 5], ["K" => 1, "k" => 5], $vcb, $icase));
var_dump(array_uintersect_uassoc(["k" => 5], ["K" => 1, "k" => 5], $vcb, $icase));
var_dump(array_diff_uassoc(["a" => 1, "A" => 2], ["A" => 1, "a" => 2], $icase));
var_dump(array_intersect_uassoc(["a" => 1, "A" => 2], ["A" => 1, "a" => 2], $icase));
--EXPECT--
array(0) {
}
array(1) {
  ["k"]=>
  int(5)
}
array(0) {
}
array(1) {
  ["k"]=>
  int(5)
}
array(0) {
}
array(2) {
  ["a"]=>
  int(1)
  ["A"]=>
  int(2)
}
