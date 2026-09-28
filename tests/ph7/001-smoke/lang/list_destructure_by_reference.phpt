--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A destructuring target may bind BY REFERENCE, and php compiles it as `=&`
--DESCRIPTION--
`[$a, &$b] = $src` binds $b to `$src[1]` rather than copying the element out, so a
later write through $b lands in the array -- php compiles the entry as the
`$b =& $src[1]` it means. Every spelling takes it: the short form and `list()`,
the KEYED form, a nested level, and a foreach `as` list, where the row itself has
to be the array ELEMENT for the bind to reach anything. PHL refused all of them
with `Assignments can only happen to writable values`.

Three rules come with the bind. A MISSING key is vivified in the source and stays
SILENT, exactly as `$t =& $src[k]` creates what it binds to. The entries are
settled in SOURCE ORDER, which is only visible when they alias each other:
`[&$y, $y] = [1, 2]` binds $y to element 0 and then assigns element 1's value to
$y, which lands in element 0. And a target may be anything a write may reach --
a property, an array element, a static.
--FILE--
<?php
/* Positional, both spellings. */
$refA = [1, 2, 3];
[$refX, &$refY] = $refA;
$refY = 'Y';
var_dump($refA, $refX);

$refB = [1, 2];
list(&$refP, $refQ) = $refB;
$refP = 'P';
var_dump($refB, $refQ);

/* Keyed. */
$refC = ['k' => 1, 'j' => 2];
['k' => &$refK, 'j' => $refJ] = $refC;
$refK = 'K';
var_dump($refC, $refJ);

/* Nested, and a skipped slot. */
$refD = [[1, 2], 3];
[[&$refN1, $refN2], $refN3] = $refD;
$refN1 = 'N';
var_dump($refD);

$refH = [1, 2, 3];
[, &$refH2] = $refH;
$refH2 = 'H';
var_dump($refH);

/* foreach: the row is the element, so the bind reaches the array. */
$refE = [[1, 2], [3, 4]];
foreach ($refE as [&$refF1, $refF2]) { $refF1 *= 10; }
unset($refF1);
var_dump($refE);

/* A missing key is created, silently. */
$refG = [1];
[$refM1, &$refM2] = $refG;
var_dump($refG);

/* Any write target. */
class RefListHolder { public $prop; public static $stat; }
$refObj = new RefListHolder();
$refI = [5];
[&$refObj->prop] = $refI;
$refObj->prop = 'O';
var_dump($refI);

$refJ2 = [0];
$refK2 = [9];
[&$refJ2[0]] = $refK2;
$refJ2[0] = 'J';
var_dump($refK2);

/* Source order: the second entry writes through the first entry's bind. */
$refF = [1, 2];
[&$refSame, $refSame] = $refF;
var_dump($refF, $refSame);

/* Two binds, one target: the last one wins, as a second `=&` would. */
$refL = [1, 2, 3];
[&$refOne, &$refOne] = $refL;
var_dump($refL, $refOne);
?>
--EXPECT--
array(3) {
  [0]=>
  int(1)
  [1]=>
  &string(1) "Y"
  [2]=>
  int(3)
}
int(1)
array(2) {
  [0]=>
  &string(1) "P"
  [1]=>
  int(2)
}
int(2)
array(2) {
  ["k"]=>
  &string(1) "K"
  ["j"]=>
  int(2)
}
int(2)
array(2) {
  [0]=>
  array(2) {
    [0]=>
    &string(1) "N"
    [1]=>
    int(2)
  }
  [1]=>
  int(3)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  &string(1) "H"
  [2]=>
  int(3)
}
array(2) {
  [0]=>
  array(2) {
    [0]=>
    int(10)
    [1]=>
    int(2)
  }
  [1]=>
  array(2) {
    [0]=>
    int(30)
    [1]=>
    int(4)
  }
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  &NULL
}
array(1) {
  [0]=>
  &string(1) "O"
}
array(1) {
  [0]=>
  &string(1) "J"
}
array(2) {
  [0]=>
  &int(2)
  [1]=>
  int(2)
}
int(2)
array(3) {
  [0]=>
  int(1)
  [1]=>
  &int(2)
  [2]=>
  int(3)
}
int(2)
