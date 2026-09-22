--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
SORT_NUMERIC compares operands as DOUBLES, ties above 2^53 included
--DESCRIPTION--
php's numeric_compare_function is `zval_get_double(a)` against `zval_get_double(b)` under
ZEND_THREEWAY_COMPARE -- not a value comparison over whatever type each operand settles on.
PHL folded both operands to a NUMBER and called the ordinary comparator, which differed in two
observable ways: an object warned `could not be converted to int` where php says `to float`,
and two integers a double cannot tell apart were ordered EXACTLY, so
`sort([PHP_INT_MAX, PHP_INT_MAX-1], SORT_NUMERIC)` re-ordered a pair php's stable sort leaves
alone. Matching php means adopting its precision loss above 2^53, which is what parity is
(§10). NaN ordering is deliberately not asserted: `ZEND_THREEWAY_COMPARE` answers 1 for every
NaN comparison in both engines, so the resulting POSITION is whatever each sort algorithm does
with a non-total order, and php does not define it.

The error handler is used so the assertion matches the message BODY on both engines regardless
of the log-copy prefix (§6).
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

class SnBare {}

echo "== integers a double cannot tell apart TIE, so a stable sort leaves them\n";
$a = [PHP_INT_MAX, PHP_INT_MAX - 1, PHP_INT_MAX - 2];
sort($a, SORT_NUMERIC);
var_dump($a);
$b = [PHP_INT_MAX - 2, PHP_INT_MAX, PHP_INT_MAX - 1];
sort($b, SORT_NUMERIC);
var_dump($b);

echo "== ...but values a double CAN tell apart still order\n";
$c = [30, 3, "20", 2.5, true, null, "1e2", -1];
sort($c, SORT_NUMERIC);
var_dump($c);

echo "== a non-numeric string is 0.0\n";
$d = ["abc", 1, -1];
sort($d, SORT_NUMERIC);
var_dump($d);

echo "== the infinities sit at the ends\n";
$e = [1, INF, -INF, 0];
sort($e, SORT_NUMERIC);
var_dump($e);

echo "== an object warns \"to float\" per comparison and sorts as 1.0\n";
$o = [3, new SnBare(), 1];
sort($o, SORT_NUMERIC);
var_dump($o);

echo "== the whole flag family shares the comparator\n";
foreach (['rsort', 'asort', 'arsort'] as $fn) {
    $x = [PHP_INT_MAX, PHP_INT_MAX - 1, 5, "4"];
    $fn($x, SORT_NUMERIC);
    echo $fn, ": ", implode(",", array_keys($x)), " | ", implode(",", $x), "\n";
}
$k = ["9" => 'a', "10" => 'b', "1e2" => 'c'];
ksort($k, SORT_NUMERIC);
echo "ksort: ", implode(",", array_keys($k)), "\n";

echo "== array_unique(SORT_NUMERIC) uses it too\n";
var_dump(array_unique([1, "1", 1.0, "1.0", 2, PHP_INT_MAX, PHP_INT_MAX - 1], SORT_NUMERIC));
?>
--EXPECT--
== integers a double cannot tell apart TIE, so a stable sort leaves them
array(3) {
  [0]=>
  int(9223372036854775807)
  [1]=>
  int(9223372036854775806)
  [2]=>
  int(9223372036854775805)
}
array(3) {
  [0]=>
  int(9223372036854775805)
  [1]=>
  int(9223372036854775807)
  [2]=>
  int(9223372036854775806)
}
== ...but values a double CAN tell apart still order
array(8) {
  [0]=>
  int(-1)
  [1]=>
  NULL
  [2]=>
  bool(true)
  [3]=>
  float(2.5)
  [4]=>
  int(3)
  [5]=>
  string(2) "20"
  [6]=>
  int(30)
  [7]=>
  string(3) "1e2"
}
== a non-numeric string is 0.0
array(3) {
  [0]=>
  int(-1)
  [1]=>
  string(3) "abc"
  [2]=>
  int(1)
}
== the infinities sit at the ends
array(4) {
  [0]=>
  float(-INF)
  [1]=>
  int(0)
  [2]=>
  int(1)
  [3]=>
  float(INF)
}
== an object warns "to float" per comparison and sorts as 1.0
  [2] Object of class SnBare could not be converted to float
  [2] Object of class SnBare could not be converted to float
array(3) {
  [0]=>
  object(SnBare)#2 (0) {
  }
  [1]=>
  int(1)
  [2]=>
  int(3)
}
== the whole flag family shares the comparator
rsort: 0,1,2,3 | 9223372036854775807,9223372036854775806,5,4
asort: 3,2,0,1 | 4,5,9223372036854775807,9223372036854775806
arsort: 0,1,2,3 | 9223372036854775807,9223372036854775806,5,4
ksort: 9,10,1e2
== array_unique(SORT_NUMERIC) uses it too
array(3) {
  [0]=>
  int(1)
  [4]=>
  int(2)
  [5]=>
  int(9223372036854775807)
}
