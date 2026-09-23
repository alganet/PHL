--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_multisort applies one permutation to every column, ties falling to the next column
--FILE--
<?php
// One permutation applied to every column; a tie in one column falls through
// to the next, each column under its own order and flags.
$data = [3, 1, 2];
$tag = ["c", "a", "b"];
var_dump(array_multisort($data, $tag));
var_dump($data, $tag);
$c1 = [1, 1, 0];
$c2 = ["b", "a", "z"];
array_multisort($c1, $c2);
var_dump($c1, $c2);
$q = [1, 1, 2];
$r = [2, 1, 1];
$s = ["a", "b", "c"];
array_multisort($q, SORT_ASC, $r, SORT_DESC, $s);
var_dump($q, $r, $s);
// order and flags attach to the preceding array, in either order
$m = [2, 1, 10];
array_multisort($m, SORT_STRING, SORT_DESC);
var_dump($m);
// string keys are kept, numeric keys renumbered
$k = ["k" => 2, 5 => 1, "j" => 3];
array_multisort($k);
var_dump($k);
?>
--EXPECT--
bool(true)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
array(3) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
  [2]=>
  string(1) "c"
}
array(3) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(1)
}
array(3) {
  [0]=>
  string(1) "z"
  [1]=>
  string(1) "a"
  [2]=>
  string(1) "b"
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(1)
  [2]=>
  int(2)
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(1)
  [2]=>
  int(1)
}
array(3) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
  [2]=>
  string(1) "c"
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(10)
  [2]=>
  int(1)
}
array(3) {
  [0]=>
  int(1)
  ["k"]=>
  int(2)
  ["j"]=>
  int(3)
}
