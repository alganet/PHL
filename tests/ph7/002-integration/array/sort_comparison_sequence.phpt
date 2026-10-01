--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Which pairs a sort compares, and how many, is the same sequence php spends — and where NAN lands falls out of it
--DESCRIPTION--
A sort's comparison sequence is not an implementation detail. A comparator that
counts, logs or throws is reading the engine's own decisions, and a comparison
that is not an ordering — NAN answers 1 against everything in both directions —
has no answer at all except the one the algorithm happens to produce.

PHL ordered over a stable bottom-up merge; php runs the hybrid insertion/quick
sort of `zend_sort`. The two agree on the ANSWER for every consistent
comparator — 414 counted sorts over sizes 0 to 257 differed on no result at all
— and disagree on both of the things above: 336 of those 414 spent a different
number of comparisons, and `sort([3, NAN, 1, 2])` answered `1 2 NAN 3` against
php's `NAN 1 2 3`.

Stability is php's too, and is NOT a property of that sort — a quicksort
reorders equal elements freely. php's sorts have been stable since 8.0 because
every element is stamped with its position before the sort runs and the sort
builtins' comparators fall back on that stamp when the real comparison answers
0, so the user comparator is never entered for the tiebreak.
--FILE--
<?php
/* 1. The comparison is not an ordering. NAN answers 1 against everything in
 *    both directions, so where NAN lands is decided by the ALGORITHM alone —
 *    a merge sort and php's quicksort have no reason to agree. */
var_dump(NAN <=> 1, 1 <=> NAN);
foreach ([[3, NAN, 1, 2], [NAN, 3, 1, 2], [3, 1, 2, NAN], [NAN, NAN, 2, 1]] as $case) {
    $a = $case;
    sort($a);
    echo implode(' ', array_map(fn ($v) => is_nan($v) ? 'NAN' : $v, $a)), "\n";
}

/* 2. A comparator that counts is reading the engine's own decisions, so both
 *    HOW MANY pairs it is handed and WHICH pairs they are is contract. The
 *    sizes below straddle every branch of php's hybrid: the fixed networks at
 *    two to five, the linear insertion scan up to six, the two-at-a-time scan
 *    after it, and the quicksort past sixteen. */
foreach ([2, 3, 4, 5, 6, 7, 8, 16, 17, 24] as $n) {
    $a = [];
    for ($i = 0; $i < $n; $i++) {
        $a[] = ($i * 7 + 3) % $n;   /* a fixed permutation, no randomness */
    }
    $pairs = [];
    $b = $a;
    usort($b, function ($x, $y) use (&$pairs) { $pairs[] = "$x:$y"; return $x <=> $y; });
    printf("n=%2d calls=%2d %s\n", $n, count($pairs), implode(' ', $pairs));
}

/* 3. php's sort is a quicksort and reorders equal elements freely; it is STABLE
 *    because every element is stamped with its position first and the stamp
 *    breaks a tie the comparator calls equal. So equal elements keep their
 *    original order, and the comparator is never entered for the tiebreak. */
$c = [];
for ($i = 0; $i < 40; $i++) {
    $c["k$i"] = $i % 3;
}
$calls = 0;
uasort($c, function ($x, $y) use (&$calls) { $calls++; return $x <=> $y; });
echo implode(',', array_keys($c)), "\n";
var_dump($calls);

/* 4. And the ties a FLAG sort calls equal keep their order too. */
$d = ['10', '9', '10.0', '9.0', '1e1', '0x9'];
sort($d, SORT_NUMERIC);
var_dump($d);
?>
--EXPECT--
int(1)
int(1)
NAN 1 2 3
1 2 3 NAN
NAN 1 2 3
1 NAN 2 NAN
n= 2 calls= 1 1:0
n= 3 calls= 2 0:1 1:2
n= 4 calls= 5 3:2 1:2 3:0 2:0 1:0
n= 5 calls= 8 3:0 2:0 3:2 3:4 4:1 3:1 2:1 0:1
n= 6 calls=13 3:4 4:5 5:0 4:0 3:0 5:1 4:1 3:1 0:1 5:2 4:2 3:2 1:2
n= 7 calls= 6 3:3 3:3 3:3 3:3 3:3 3:3
n= 8 calls=16 3:2 3:1 2:1 3:0 2:0 1:0 3:7 7:6 3:6 7:5 3:5 6:5 7:4 5:4 2:4 3:4
n=16 calls=56 3:10 10:1 3:1 10:8 3:8 10:15 15:6 10:6 8:6 3:6 15:13 8:13 10:13 15:4 10:4 6:4 1:4 3:4 15:11 10:11 13:11 15:2 11:2 8:2 4:2 1:2 3:2 15:9 11:9 8:9 10:9 15:0 11:0 9:0 6:0 3:0 1:0 15:7 11:7 9:7 6:7 8:7 15:14 11:14 13:14 15:5 13:5 10:5 8:5 6:5 3:5 4:5 15:12 13:12 10:12 11:12
n=17 calls=53 3:8 8:13 8:0 8:7 8:14 6:8 8:4 8:11 16:8 9:8 2:8 8:1 8:10 12:8 5:8 8:15 15:10 15:12 10:12 15:11 12:11 10:11 15:9 12:9 11:9 10:9 15:16 16:14 12:14 15:14 16:13 14:13 11:13 12:13 3:5 5:0 3:0 5:7 7:6 5:6 7:4 6:4 5:4 3:4 7:2 5:2 3:2 2:0 7:1 5:1 3:1 0:1 2:1
n=24 calls=97 3:15 15:20 15:17 13:15 15:0 15:7 15:14 15:21 6:15 15:4 15:11 15:18 23:15 16:15 9:15 15:1 15:8 15:10 15:22 2:15 15:5 15:12 15:19 19:22 22:18 19:18 22:16 19:16 18:16 22:23 23:21 22:21 19:21 23:17 21:17 18:17 17:16 23:20 21:20 18:20 19:20 3:12 12:13 13:0 12:0 3:0 13:7 12:7 3:7 13:14 14:6 12:6 3:6 7:6 14:4 12:4 6:4 0:4 3:4 14:11 12:11 6:11 7:11 14:9 12:9 7:9 11:9 14:1 12:1 9:1 6:1 3:1 1:0 14:8 12:8 9:8 6:8 7:8 14:10 12:10 9:10 11:10 14:2 12:2 10:2 8:2 6:2 3:2 0:2 1:2 14:5 12:5 10:5 8:5 6:5 3:5 4:5
k0,k3,k6,k9,k12,k15,k18,k21,k24,k27,k30,k33,k36,k39,k1,k4,k7,k10,k13,k16,k19,k22,k25,k28,k31,k34,k37,k2,k5,k8,k11,k14,k17,k20,k23,k26,k29,k32,k35,k38
int(176)
array(6) {
  [0]=>
  string(3) "0x9"
  [1]=>
  string(1) "9"
  [2]=>
  string(3) "9.0"
  [3]=>
  string(2) "10"
  [4]=>
  string(4) "10.0"
  [5]=>
  string(3) "1e1"
}
--CLEAN--
<?php
