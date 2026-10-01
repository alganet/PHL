--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A comparator that touches the array being sorted: reading it answers the array as it was at the call, and writing to it changes nothing (it used to be a segmentation fault)
--DESCRIPTION--
php sorts a private copy of the array — `zend_array_dup()` in `php_usort()` and
its two siblings — so a comparator that reads the array being sorted always sees
it exactly as it was at the call, and a write it makes to that array is
discarded when the sorted copy is installed over it.

PHL threaded its merge through the nodes' own links, which are the map's ONLY
list. While the sort ran, the map's head led into half-merged runs, so a
comparator that so much as printed the array walked garbage and the interpreter
died: `usort($a, function ($x, $y) use (&$a) { echo implode(',', $a); ... })` was
a segmentation fault, not a wrong answer, and so was the same read from a
`catch` around a comparator that throws. An `unset($a[0])` in there freed a node
the sort was still holding.

The order is decided over a vector of node pointers now and the list is relinked
once, after the last comparison, so the array stays walkable in its pre-call
order for as long as user code can see it. The map is lent one extra reference
for the duration, so a write through the variable copy-on-write separates a map
of its own, and the sorted map is installed over whatever the comparator left
behind.

How MANY comparisons an engine spends is not part of this — the snapshots below
are collected as array keys so the row measures what the comparator saw, not how
often it ran.
--FILE--
<?php
/* 1. A comparator that READS the array being sorted sees it as it was at the
 *    call — every time, for every one of the three user-comparator sorts. The
 *    snapshots are collected as KEYS so the row does not depend on how many
 *    comparisons an engine spends. */
$a = [5, 4, 3, 2, 1];
$seen = [];
usort($a, function ($x, $y) use (&$a, &$seen) { $seen[implode(',', $a)] = 1; return $x <=> $y; });
var_dump(array_keys($seen));
echo implode(',', $a), "\n";

$b = ['x' => 5, 'y' => 4, 'z' => 3];
$seen = [];
uasort($b, function ($p, $q) use (&$b, &$seen) { $seen[implode(',', array_keys($b))] = 1; return $p <=> $q; });
var_dump(array_keys($seen));
echo implode(',', array_keys($b)), "\n";

$c = ['c' => 1, 'a' => 2, 'b' => 3];
$seen = [];
uksort($c, function ($p, $q) use (&$c, &$seen) { $seen[implode(',', array_keys($c))] = 1; return strcmp($p, $q); });
var_dump(array_keys($seen));
echo implode(',', array_keys($c)), "\n";

/* 2. A WRITE the comparator makes to the array being sorted is discarded. */
$d = [3, 1, 2]; $n = 0;
usort($d, function ($x, $y) use (&$d, &$n) { if (++$n == 1) { $d[] = 99; } return $x <=> $y; });
echo implode(',', $d), "\n";

$e = [3, 1, 2]; $n = 0;
usort($e, function ($x, $y) use (&$e, &$n) { if (++$n == 1) { unset($e[0]); } return $x <=> $y; });
echo implode(',', $e), "\n";

$f = [3, 1, 2]; $n = 0;
usort($f, function ($x, $y) use (&$f, &$n) { if (++$n == 1) { $f[0] = 77; } return $x <=> $y; });
echo implode(',', $f), "\n";

$g = [3, 1, 2]; $n = 0;
usort($g, function ($x, $y) use (&$g, &$n) { if (++$n == 1) { $g = 'gone'; } return $x <=> $y; });
var_dump($g);

/* A whole nested sort of the same array from inside the comparator is a write
 * like any other. */
$h = [3, 1, 2]; $n = 0;
usort($h, function ($x, $y) use (&$h, &$n) { if (++$n == 1) { usort($h, fn ($p, $q) => $q <=> $p); } return $x <=> $y; });
echo implode(',', $h), "\n";

/* 3. A comparator that throws on its FIRST comparison: the catch — which this
 *    engine runs before the sort's C frame unwinds — reads a walkable array. */
$i = [5, 4, 3, 2, 1];
try {
    usort($i, function ($x, $y) { throw new RuntimeException('stop'); });
} catch (RuntimeException $ex) {
    echo 'in catch: ', implode(',', $i), "\n";
}
echo 'after: ', implode(',', $i), "\n";
?>
--EXPECT--
array(1) {
  [0]=>
  string(9) "5,4,3,2,1"
}
1,2,3,4,5
array(1) {
  [0]=>
  string(5) "x,y,z"
}
z,y,x
array(1) {
  [0]=>
  string(5) "c,a,b"
}
a,b,c
1,2,3
1,2,3
1,2,3
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
1,2,3
in catch: 5,4,3,2,1
after: 5,4,3,2,1
--CLEAN--
<?php
