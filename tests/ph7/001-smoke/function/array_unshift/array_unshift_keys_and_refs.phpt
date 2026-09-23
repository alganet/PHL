--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() keeps a referenced element, renumbers only the integer keys, and refuses a literal
--FILE--
<?php
// The embedded-PHP implementation rebuilt the array through
// array_copy/array_erase/array_merge, and a REFERENCED element did not survive
// that round trip: it was dropped from the array entirely.
$auV = 9;
$auA = [1, &$auV, 3];
var_dump(array_unshift($auA, 0));
$auV = 10;
var_dump($auA);

// Integer keys are renumbered in iteration order; string keys keep theirs and
// stay where they are.
$auB = ['x' => 1, 5 => 2, '7' => 3, 'b' => 4];
var_dump(array_unshift($auB, 'z'));
var_dump($auB);

// The renumbering happens even when nothing is prepended.
$auC = [5 => 'a', 9 => 'b'];
var_dump(array_unshift($auC), $auC);

// Several values keep the order they were written, and the internal pointer is
// reset to the new first element.
$auD = [3];
var_dump(array_unshift($auD, 1, 2), $auD);
next($auD);
array_unshift($auD, 0);
var_dump(current($auD));

// A negative key is renumbered like any other integer key.
$auE = [-5 => 'a', -1 => 'b'];
array_unshift($auE, 'z');
var_dump($auE);

// php's by-reference parameter: a literal has nowhere to write back to.
try { array_unshift([1], 2); } catch (Error $e) { echo $e->getMessage(), "\n"; }
foreach ([5, null, "s", new stdClass] as $auBad) {
    try { array_unshift($auBad, 1); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
int(4)
array(4) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  &int(10)
  [3]=>
  int(3)
}
int(5)
array(5) {
  [0]=>
  string(1) "z"
  ["x"]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  ["b"]=>
  int(4)
}
int(2)
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
}
int(3)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
int(0)
array(3) {
  [0]=>
  string(1) "z"
  [1]=>
  string(1) "a"
  [2]=>
  string(1) "b"
}
array_unshift(): Argument #1 ($array) could not be passed by reference
array_unshift(): Argument #1 ($array) must be of type array, int given
array_unshift(): Argument #1 ($array) must be of type array, null given
array_unshift(): Argument #1 ($array) must be of type array, string given
array_unshift(): Argument #1 ($array) must be of type array, stdClass given
--CLEAN--
<?php
