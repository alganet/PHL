--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array literal entry keeps the value it had when it was written
--FILE--
<?php
echo "-- every entry is the value it had when it was written\n";
$x = 'first';
var_dump([$x, $x = 'secnd']);

echo "-- the assignment is longer than the entry it must not reach\n";
$x = 'first';
var_dump([$x, $x = 'muchlonger']);

echo "-- array element, property, static property, variable variable\n";
$a = ['first'];
var_dump([$a[0], $a[0] = 'secnd']);
class P { public $p = 'first'; public static $s = 'first'; }
$o = new P();
var_dump([$o->p, $o->p = 'secnd']);
var_dump([P::$s, P::$s = 'secnd']);
$x = 'first';
$nm = 'x';
var_dump([$$nm, $x = 'secnd']);

echo "-- a KEY is an entry too, and two keys used to collide\n";
$x = 'first';
var_dump([$x => 1, ($x = 'secnd') => 2]);

echo "-- the writer does not have to be an assignment\n";
$x = 'first';
function bump(&$r) { $r = 'secnd'; return 'B'; }
var_dump([$x, bump($x)]);
$x = 'first';
var_dump([$x, (function () use (&$x) { $x = 'secnd'; return 'C'; })()]);

echo "-- entries after the writer see what it did\n";
$x = 'first';
var_dump([$x, $x = 'secnd', $x]);

echo "-- with a key on every entry\n";
$x = 'first';
var_dump(['a' => $x, 'b' => $x = 'secnd']);

echo "-- a by-reference entry still tracks its variable\n";
$x = 'first';
$r = [&$x, $x = 'secnd'];
var_dump($r);

echo "-- and a nested literal is a value of its own\n";
$x = 'first';
var_dump([[$x], $x = 'secnd']);

echo "-- nothing runs after the first entry, nothing is copied\n";
$x = 'first';
var_dump([$x, $x, 1, 'z']);
?>
--EXPECT--
-- every entry is the value it had when it was written
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(5) "secnd"
}
-- the assignment is longer than the entry it must not reach
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(10) "muchlonger"
}
-- array element, property, static property, variable variable
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(5) "secnd"
}
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(5) "secnd"
}
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(5) "secnd"
}
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(5) "secnd"
}
-- a KEY is an entry too, and two keys used to collide
array(2) {
  ["first"]=>
  int(1)
  ["secnd"]=>
  int(2)
}
-- the writer does not have to be an assignment
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(1) "B"
}
array(2) {
  [0]=>
  string(5) "first"
  [1]=>
  string(1) "C"
}
-- entries after the writer see what it did
array(3) {
  [0]=>
  string(5) "first"
  [1]=>
  string(5) "secnd"
  [2]=>
  string(5) "secnd"
}
-- with a key on every entry
array(2) {
  ["a"]=>
  string(5) "first"
  ["b"]=>
  string(5) "secnd"
}
-- a by-reference entry still tracks its variable
array(2) {
  [0]=>
  &string(5) "secnd"
  [1]=>
  string(5) "secnd"
}
-- and a nested literal is a value of its own
array(2) {
  [0]=>
  array(1) {
    [0]=>
    string(5) "first"
  }
  [1]=>
  string(5) "secnd"
}
-- nothing runs after the first entry, nothing is copied
array(4) {
  [0]=>
  string(5) "first"
  [1]=>
  string(5) "first"
  [2]=>
  int(1)
  [3]=>
  string(1) "z"
}
