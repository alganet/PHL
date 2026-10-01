--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The callback-taking diff/intersect members enter their callback the number of times php does, on the pairs php hands it
--DESCRIPTION--
php answers these ten by SORTING a private list of every argument array once
with the comparison the member is defined over and then MERGING the sorted
lists. PHL rescanned a whole comparand array for every source entry instead: the
two agree on the answer for every consistent comparator — 40320 swept calls over
the ten members differed on no result at all — and disagree on how many times
the callback is entered and on which pairs it is handed.
`array_udiff([1, 2, 3], [2, 3, 4], $f)` entered $f six times here and nine times
in php. A callback that counts, logs or throws is reading the engine's own
decisions, so the sequence is contract; the merge is also O(n log n) where the
rescan was O(n*m).

Two of the ten are not that algorithm at all: php answers array_udiff_assoc()
and array_uintersect_assoc() through its key-hash pair, which sorts nothing and
never compares the values of an entry whose key is missing.

The merge holds the operand arrays for as long as user code can run between its
comparisons, so each of them is lent one reference: a write the callback makes
through the caller's variable copy-on-write separates a map of its own.
--FILE--
<?php
$log = [];
function trace(string $tag, callable $run): void
{
    global $log;
    $log = [];
    $r = $run();
    printf("%-24s %2d  %s => %s\n", $tag, count($log), implode(' ', $log), json_encode($r));
}
$v = function ($x, $y) { global $log; $log[] = "v($x,$y)"; return $x <=> $y; };
$k = function ($x, $y) { global $log; $log[] = "k($x,$y)"; return $x <=> $y; };

/* 1. The value-matched members sort every operand with the callback and merge
 *    the sorted lists. A rescan of each operand per source entry answers the
 *    same array and enters the callback a different number of times. */
$a = [1, 2, 3];
$b = [2, 3, 4];
trace('array_udiff', fn () => array_udiff($a, $b, $v));
trace('array_uintersect', fn () => array_uintersect($a, $b, $v));
trace('array_udiff/3', fn () => array_udiff($a, $b, [3, 9], $v));
trace('array_udiff/dups', fn () => array_udiff([5, 5, 1, 5], [5], $v));

/* 2. The key-matched members sort by KEY instead, and _assoc then asks the
 *    value of a key-matched pair. */
$c = ['x' => 1, 'y' => 2, 'z' => 3];
$d = ['y' => 2, 'z' => 9, 'w' => 4];
trace('array_diff_ukey', fn () => array_diff_ukey($c, $d, $k));
trace('array_intersect_ukey', fn () => array_intersect_ukey($c, $d, $k));
trace('array_diff_uassoc', fn () => array_diff_uassoc($c, $d, $k));
trace('array_intersect_uassoc', fn () => array_intersect_uassoc($c, $d, $k));

/* 3. array_udiff_assoc() and array_uintersect_assoc() are NOT that algorithm.
 *    php answers them through its key-hash pair, which sorts nothing: the
 *    callback sees $array1 in its own order, and an entry whose key is missing
 *    from an operand costs no comparison at all. */
$e = ['b' => 2, 'a' => 1, 'c' => 3];
$f = ['b' => 2, 'a' => 9];
trace('array_udiff_assoc', fn () => array_udiff_assoc($e, $f, $v));
trace('array_uintersect_assoc', fn () => array_uintersect_assoc($e, $f, $v));

/* 4. php holds ONE live comparison callback and swaps it between the key one
 *    and the value one, restoring the key one only where the value comparison
 *    answered UNEQUAL. So after a key-and-value match, the next operand's KEY
 *    comparison is made with the VALUE callback -- the third and fourth entries
 *    below hand $v a pair of KEYS. array_uintersect_uassoc() is the only member
 *    that can show it: every other one has at most one user callback, and the
 *    diff side leaves the loop at that point. */
trace('array_uintersect_uassoc', fn () => array_uintersect_uassoc([5], [5], [5], $v, $k));
trace('array_udiff_uassoc', fn () => array_udiff_uassoc([5], [5], [5], $v, $k));

/* 5. A callback that writes to an operand array changes nothing the merge is
 *    walking: it holds the arrays as they were at the call. */
$g = [5, 3, 1];
$h = [3, 1, 9];
$n = 0;
$r = array_udiff($g, $h, function ($x, $y) use (&$g, &$h, &$n) {
    if (++$n == 1) { $h[] = 77; unset($h[0]); $g[] = 88; }
    return $x <=> $y;
});
echo json_encode($r), ' g=', json_encode($g), ' h=', json_encode($h), "\n";
?>
--EXPECT--
array_udiff               9  v(1,2) v(2,3) v(2,3) v(3,4) v(1,2) v(1,2) v(2,2) v(2,3) v(3,3) => [1]
array_uintersect          9  v(1,2) v(2,3) v(2,3) v(3,4) v(1,2) v(2,2) v(2,2) v(2,3) v(3,3) => {"1":2,"2":3}
array_udiff/3            11  v(1,2) v(2,3) v(2,3) v(3,4) v(3,9) v(1,2) v(1,3) v(1,2) v(2,2) v(2,3) v(3,3) => [1]
array_udiff/dups          9  v(5,5) v(5,1) v(5,1) v(5,5) v(1,5) v(1,5) v(5,5) v(5,5) v(5,5) => {"2":1}
array_diff_ukey          13  k(x,y) k(y,z) k(y,z) k(z,w) k(y,w) k(x,w) k(x,y) k(x,z) k(y,w) k(y,y) k(z,w) k(z,y) k(z,z) => {"x":1}
array_intersect_ukey      9  k(x,y) k(y,z) k(y,z) k(z,w) k(y,w) k(x,w) k(x,y) k(y,y) k(z,z) => {"y":2,"z":3}
array_diff_uassoc        13  k(x,y) k(y,z) k(y,z) k(z,w) k(y,w) k(x,w) k(x,y) k(x,z) k(y,w) k(y,y) k(z,w) k(z,y) k(z,z) => {"x":1,"z":3}
array_intersect_uassoc    9  k(x,y) k(y,z) k(y,z) k(z,w) k(y,w) k(x,w) k(x,y) k(y,y) k(z,z) => {"y":2}
array_udiff_assoc         2  v(2,2) v(1,9) => {"a":1,"c":3}
array_uintersect_assoc    2  v(2,2) v(1,9) => {"b":2}
array_uintersect_uassoc   4  k(0,0) v(5,5) v(0,0) v(5,5) => [5]
array_udiff_uassoc        2  k(0,0) v(5,5) => []
[5] g=[5,3,1,88] h={"1":1,"2":9,"3":77}
--CLEAN--
<?php
