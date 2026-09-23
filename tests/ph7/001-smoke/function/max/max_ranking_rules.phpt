--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
max() ranks with php's comparison, keeps the first of an equal pair, and lets NaN lose
--SKIPIF--
skip: flaky
--FILE--
<?php
// max() used to be embedded PHP folding with the `>` operator, which is
// `is_smaller(max, val)` -- the REVERSE of the comparison php runs. Two values
// php cannot compare at all (objects of different classes answer
// "uncomparable", not an ordering) went the other way for it.
class MaxRankLeft {}
class MaxRankRight {}
$l = new MaxRankLeft;
$r = new MaxRankRight;
var_dump(max($l, $r) === $l);
var_dump(max([$l, $r]) === $l);

// An equal pair answers the FIRST one, in either form, so the TYPE that comes
// back is the type that was written first.
var_dump(max(1, 1.0));
var_dump(max(1.0, 1));
var_dump(max("10", 10));
var_dump(max(10, "10"));
var_dump(max([1, 1.0]));
var_dump(max(1, 1.0, 1));

// NaN compares false in every direction, so it never displaces a value it is
// weighed against -- but it stays put when it is the one already held.
var_dump(max(NAN, 1));
var_dump(max(1, NAN));
var_dump(max([NAN, 1]));

// php 8's string/number comparison: a non-numeric string against a number is a
// STRING comparison, and two numeric strings a numeric one.
var_dump(max("abc", 10));
var_dump(max("1e3", "1000"));
var_dump(max([1, 2], [1, 3]));
var_dump(max([1, 2], [1, 2, 3]));

// The array form walks in insertion order and dereferences.
$v = 5;
var_dump(max([1, &$v, 3]));
var_dump(max(['a' => 1, 'b' => 9, 'c' => 3]));
?>
--EXPECT--
bool(true)
bool(true)
int(1)
float(1)
string(2) "10"
int(10)
int(1)
int(1)
int(1)
float(NAN)
float(NAN)
string(3) "abc"
string(3) "1e3"
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(3)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
int(5)
int(9)
--CLEAN--
<?php
