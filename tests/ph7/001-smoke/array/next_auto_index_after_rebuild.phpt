--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array rebuilt with its keys keeps its next automatic index
--FILE--
<?php
/* An array rebuilt PRESERVING keys keeps its next automatic index: php's
 * nNextFreeElement is one past the largest integer key it copied, so the next
 * `$a[] = v` lands there and not back at 0. Every builtin that rebuilds an
 * array that way has to move it — twig's `batch` filter pads a chunk by
 * appending to it, and read a `0` key into a row whose last key was 123. */
$aixSrc = ['d' => 'd', 123 => 'e'];
$aixNum = [5 => 'a', 9 => 'b'];
$aixNx = function ($a) { $a[] = 'N'; return array_key_last($a); };

$aixCases = [
    'literal'               => $aixSrc,
    'chunk preserve'        => array_chunk($aixSrc, 3, true)[0],
    'chunk renumber'        => array_chunk($aixSrc, 3, false)[0],
    'slice preserve'        => array_slice($aixSrc, 0, 2, true),
    'slice renumber'        => array_slice($aixSrc, 0, 2, false),
    'slice int keys'        => array_slice($aixNum, 0, 2, true),
    'filter'                => array_filter($aixSrc),
    'filter both'           => array_filter($aixSrc, fn ($v, $k) => true, ARRAY_FILTER_USE_BOTH),
    'reverse preserve'      => array_reverse($aixSrc, true),
    'reverse int keys'      => array_reverse($aixNum, true),
    'reverse renumber'      => array_reverse($aixSrc, false),
    'unique'                => array_unique($aixSrc),
    'diff'                  => array_diff($aixSrc, ['z']),
    'diff_key'              => array_diff_key($aixSrc, ['z' => 1]),
    'diff_assoc'            => array_diff_assoc($aixSrc, ['z' => 1]),
    'udiff'                 => array_udiff($aixSrc, ['z'], fn ($a, $b) => strcmp($a, $b)),
    'intersect'             => array_intersect($aixSrc, ['d', 'e']),
    'intersect_key'         => array_intersect_key($aixSrc, ['d' => 1, 123 => 1]),
    'intersect_assoc'       => array_intersect_assoc($aixSrc, ['d' => 'd', 123 => 'e']),
    'uintersect'            => array_uintersect($aixSrc, ['d', 'e'], fn ($a, $b) => strcmp($a, $b)),
    'pad'                   => array_pad($aixSrc, 2, 'z'),
    'union with empty'      => [] + $aixSrc,
    'union int keys'        => ['x'] + $aixNum,
    'merge'                 => array_merge($aixSrc),
    'merge_recursive'       => array_merge_recursive($aixSrc),
    'replace'               => array_replace($aixSrc),
    'combine'               => array_combine(['d', 123], ['d', 'e']),
    'flip'                  => array_flip(['d' => 'd', 0 => 123]),
    'fill_keys'             => array_fill_keys(['d', 123], 'x'),
    'fill'                  => array_fill(5, 2, 'x'),
    'column'                => array_column([['id' => 123, 'v' => 'e']], 'v', 'id'),
    'map'                   => array_map(fn ($x) => $x, $aixSrc),
    'change_key_case'       => array_change_key_case($aixSrc),
    'count_values'          => array_count_values([123, 'd']),
    'object cast'           => (array) (object) $aixSrc,
    'json_decode'           => json_decode('{"d":"d","123":"e"}', true),
    'unserialize'           => unserialize(serialize($aixSrc)),
    'iterator_to_array'     => iterator_to_array(new ArrayIterator($aixSrc), true),
    'uasort'                => (function ($a) { uasort($a, fn ($x, $y) => 0); return $a; })($aixSrc),
    'ksort'                 => (function ($a) { ksort($a); return $a; })($aixSrc),
    'copy'                  => (function ($a) { $b = $a; return $b; })($aixSrc),
];
foreach ($aixCases as $aixK => $aixV) { printf("%-20s %s\n", $aixK, var_export($aixNx($aixV), true)); }

/* The rule is one past the LARGEST int key copied, not the last one, and a
 * negative key counts (php 8.3). An occupied slot is skipped. */
echo "largest ", array_key_last((function ($a) { $a[] = 'N'; return $a; })(array_filter([7 => 'a', 2 => 'b']))), "\n";
echo "negative ", array_key_last((function ($a) { $a[] = 'N'; return $a; })(array_filter([-5 => 'a']))), "\n";
echo "strings ", array_key_last((function ($a) { $a[] = 'N'; return $a; })(array_filter(['a' => 1, 'b' => 2]))), "\n";

/* The other side of the rule: the index must still SKIP a slot something else
 * already took, and must still restart where a renumbering put it. */
$aixK = function ($a) { $a[] = 'N'; return implode(',', array_keys($a)); };
$aixHole = [];
$aixHole[2] = 'c';
$aixHole[0] = 'a';
echo 'hole      ', $aixK($aixHole), "\n";
$aixShift = [5 => 'a', 9 => 'b', 'k' => 'c'];
array_shift($aixShift);
echo 'shift     ', $aixK($aixShift), "\n";
$aixSort = [7 => 'a', 3 => 'b'];
sort($aixSort);
echo 'sort      ', $aixK($aixSort), "\n";
$aixPop = [0 => 'a', 1 => 'b', 2 => 'c'];
array_pop($aixPop);
echo 'pop       ', $aixK($aixPop), "\n";
$aixSpl = ['x', 'y', 'z'];
array_splice($aixSpl, 1, 1);
echo 'splice    ', $aixK($aixSpl), "\n";
$aixUnset = [1 => 'a'];
unset($aixUnset[1]);
$aixUnset[1] = 'b';
echo 'unset     ', $aixK($aixUnset), "\n";
$aixNeg = [-4 => 'a'];
echo 'negative  ', $aixK($aixNeg), "\n";
$aixRev = array_reverse([0 => 'a', 1 => 'b', 2 => 'c'], true);
echo 'reversed  ', $aixK($aixRev), "\n";
$aixMax = [PHP_INT_MAX => 'a'];
try { $aixMax[] = 'b'; } catch (Throwable $aixT) { echo 'saturated ', get_class($aixT), ': ', $aixT->getMessage(), "\n"; }
?>
--EXPECT--
literal              124
chunk preserve       124
chunk renumber       2
slice preserve       124
slice renumber       1
slice int keys       10
filter               124
filter both          124
reverse preserve     124
reverse int keys     10
reverse renumber     1
unique               124
diff                 124
diff_key             124
diff_assoc           124
udiff                124
intersect            124
intersect_key        124
intersect_assoc      124
uintersect           124
pad                  124
union with empty     124
union int keys       10
merge                1
merge_recursive      1
replace              124
combine              124
flip                 124
fill_keys            124
fill                 7
column               124
map                  124
change_key_case      124
count_values         124
object cast          124
json_decode          124
unserialize          124
iterator_to_array    124
uasort               124
ksort                124
copy                 124
largest 8
negative -4
strings 0
hole      2,0,3
shift     0,k,1
sort      0,1,2
pop       0,1,2
splice    0,1,2
unset     1,2
negative  -4,-3
reversed  2,1,0,3
saturated Error: Cannot add element to the array as the next element is already occupied
