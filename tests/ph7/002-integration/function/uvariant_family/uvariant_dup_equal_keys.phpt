--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A comparand array holding two keys its own callback equates: which candidate's value is consulted is the merge's tie order, and it is php's
--DESCRIPTION--
This used to be a recorded divergence, in a twin pair, on the reasoning that the
answer is an artifact of php's zend_sort tie order and that real comparators
over real data never present two callback-equal keys in one array. PHL scanned
every callback-equal candidate and dropped the entry when ANY of them also
matched its value; php sorts each operand once and consults the single candidate
its merge lines up.

The tie order stopped being an artifact when the merge became php's: the whole
family answers over the sort php sorts with, so the candidate it lines up is the
same candidate. Both halves of the pair are this one file now.
--FILE--
<?php
// A comparand array holding TWO keys the callback itself equates ("K" and "k"
// under strcasecmp). Which candidate's VALUE gets compared falls out of where
// the sort put the two of them.
$icase = fn($a, $b) => strcasecmp((string)$a, (string)$b);
$vcb = fn($a, $b) => $a <=> $b;
var_dump(array_diff_uassoc(["k" => 5], ["K" => 1, "k" => 5], $icase));
var_dump(array_intersect_uassoc(["k" => 5], ["K" => 1, "k" => 5], $icase));
var_dump(array_udiff_uassoc(["k" => 5], ["K" => 1, "k" => 5], $vcb, $icase));
var_dump(array_uintersect_uassoc(["k" => 5], ["K" => 1, "k" => 5], $vcb, $icase));
var_dump(array_diff_uassoc(["a" => 1, "A" => 2], ["A" => 1, "a" => 2], $icase));
var_dump(array_intersect_uassoc(["a" => 1, "A" => 2], ["A" => 1, "a" => 2], $icase));
--EXPECT--
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
array(1) {
  ["A"]=>
  int(2)
}
array(2) {
  ["a"]=>
  int(1)
  ["A"]=>
  int(2)
}
