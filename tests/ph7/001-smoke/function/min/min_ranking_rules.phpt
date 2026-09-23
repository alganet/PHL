--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
min() answers the SECOND of an equal pair at two arguments and the first at three
--SKIPIF--
skip: flaky
--FILE--
<?php
// php compiles a direct two-argument min() to a different routine from the one
// every other arity runs, and the two disagree about a tie: the pair form is
// `lhs < rhs ? lhs : rhs` (so an equal pair answers the RIGHT one) where the
// general form keeps whichever it saw first. Both are php's, in one build.
var_dump(min(1, 1.0));
var_dump(min(1.0, 1));
var_dump(min(1, 1.0, 1));
var_dump(min(1.0, 1, 1.0));
var_dump(min("10", 10));
var_dump(min(10, "10"));
var_dump(min("1e3", "1000"));
var_dump(min([1, 1.0]));

// Two values php cannot compare at all: the pair form still takes the right
// hand side, and the array form keeps the first.
class MinRankLeft {}
class MinRankRight {}
$l = new MinRankLeft;
$r = new MinRankRight;
var_dump(min($l, $r) === $r);
var_dump(min([$l, $r]) === $r);

// NaN loses the comparison in both directions, so the pair form hands back the
// operand it is weighed against.
var_dump(min(NAN, 1));
var_dump(min(1, NAN));

var_dump(min([3, 1, 2]));
var_dump(min(null, -1));
var_dump(min([1, 2], [1, 2, 3]));
?>
--EXPECT--
float(1)
int(1)
int(1)
float(1)
int(10)
string(2) "10"
string(4) "1000"
int(1)
bool(true)
bool(true)
int(1)
float(NAN)
int(1)
NULL
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
--CLEAN--
<?php
