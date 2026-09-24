--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The seeded randomisers answer php's own values: str_shuffle() permutes, shuffle() is Fisher-Yates, array_rand() samples the whole array
--FILE--
<?php
// str_shuffle() picked each output byte INDEPENDENTLY, with replacement, so what
// it answered was not a permutation of its input at all: letters repeated and
// letters went missing. The idiom that breaks is the common one -- shuffle an
// alphabet, slice a token out of it.
$rnd_src = 'abcdefghijklmnopqrstuvwxyz0123456789';
foreach ([1, 2, 42] as $rnd_seed) {
    mt_srand($rnd_seed);
    $rnd_got = str_shuffle($rnd_src);
    echo "str_shuffle($rnd_seed) = $rnd_got\n";
    echo '  is a permutation: ', var_export(count_chars($rnd_got, 1) === count_chars($rnd_src, 1), true), "\n";
}
var_dump(str_shuffle(''), str_shuffle('a'));

// shuffle() was a merge sort with a coin-flip comparator: a permutation, but not
// a uniform one and not php's. It is php's Fisher-Yates now, drawn from the same
// generator in the same order, so a seeded run answers php's permutation -- and
// it REINDEXES, dropping string keys, which php has always done.
foreach ([1, 2, 42] as $rnd_seed) {
    mt_srand($rnd_seed);
    $rnd_a = ['x' => 10, 'y' => 20, 'z' => 30, 7 => 40, 'w' => 50];
    shuffle($rnd_a);
    echo "shuffle($rnd_seed) = ", json_encode($rnd_a), "\n";
}
$rnd_one = ['only' => 1];
shuffle($rnd_one);
echo 'single element reindexes: ', json_encode($rnd_one), "\n";
$rnd_empty = [];
var_dump(shuffle($rnd_empty), $rnd_empty);

// array_rand($a, $n) with $n > 1 copied the FIRST $n keys and shuffled them --
// no sampling at all, so a "random sample" of a hundred rows was rows 0, 1 and 2
// in every run. php picks POSITIONS and then walks the array once, so the keys
// come back in the array's own ORDER and every position is reachable.
$rnd_keys = [];
for ($rnd_i = 0; $rnd_i < 20; $rnd_i++) { $rnd_keys["k$rnd_i"] = $rnd_i; }
$rnd_seen = [];
for ($rnd_seed = 1; $rnd_seed <= 40; $rnd_seed++) {
    mt_srand($rnd_seed);
    $rnd_pick = array_rand($rnd_keys, 3);
    foreach ($rnd_pick as $rnd_k) { $rnd_seen[$rnd_k] = true; }
    if ($rnd_seed <= 4) { echo "array_rand(seed $rnd_seed, 3) = ", json_encode($rnd_pick), "\n"; }
    $rnd_order = array_values($rnd_pick);
    $rnd_sorted = $rnd_order;
    usort($rnd_sorted, fn ($p, $q) => array_search($p, array_keys($rnd_keys)) <=> array_search($q, array_keys($rnd_keys)));
    if ($rnd_order !== $rnd_sorted) { echo "OUT OF ARRAY ORDER at seed $rnd_seed\n"; }
}
echo 'distinct keys reached over 40 seeds: ', count($rnd_seen), " of 20\n";

// The single-pick form walks to the drawn POSITION, and used to take one step too
// many from the far end -- so every draw in the upper half answered the key
// BEFORE the one it drew.
$rnd_hits = [];
for ($rnd_seed = 1; $rnd_seed <= 60; $rnd_seed++) {
    mt_srand($rnd_seed);
    $rnd_hits[] = array_rand($rnd_keys);
}
echo 'single picks: ', implode(',', $rnd_hits), "\n";

// ...and every $num from 1 to count() answers exactly that many keys.
foreach ([1, 2, 10, 19, 20] as $rnd_n) {
    mt_srand(7);
    $rnd_r = array_rand($rnd_keys, $rnd_n);
    echo "num=$rnd_n count=", is_array($rnd_r) ? count($rnd_r) : 1, ' ', json_encode($rnd_r), "\n";
}

// php's sorts rewind the array's internal pointer, and a ONE-element array is
// reindexed without being reordered -- which used to leave current() past the end.
$rnd_one2 = ['x' => 1];
next($rnd_one2);
shuffle($rnd_one2);
var_dump(current($rnd_one2), key($rnd_one2));
$rnd_one3 = ['only' => 5];
next($rnd_one3);
sort($rnd_one3);
var_dump(current($rnd_one3));
?>
--EXPECT--
str_shuffle(1) = g0s3bwkui41pvqdzx2acmfre8ojht795l6yn
  is a permutation: true
str_shuffle(2) = sgl8nozdk9jqfy03hc4ti2176upebm5wvxra
  is a permutation: true
str_shuffle(42) = u9jrt4xv3maybhlf6dipqnw07e8o15skz2cg
  is a permutation: true
string(0) ""
string(1) "a"
shuffle(1) = [20,30,50,40,10]
shuffle(2) = [20,10,30,50,40]
shuffle(42) = [50,10,20,40,30]
single element reindexes: [1]
bool(true)
array(0) {
}
array_rand(seed 1, 3) = ["k4","k5","k19"]
array_rand(seed 2, 3) = ["k1","k7","k8"]
array_rand(seed 3, 3) = ["k6","k8","k17"]
array_rand(seed 4, 3) = ["k10","k11","k14"]
distinct keys reached over 40 seeds: 20 of 20
single picks: k5,k8,k6,k10,k11,k10,k15,k3,k10,k17,k1,k3,k18,k15,k12,k1,k19,k6,k5,k11,k13,k9,k11,k14,k12,k9,k19,k17,k5,k17,k6,k15,k8,k13,k17,k5,k11,k17,k9,k18,k16,k2,k8,k12,k7,k13,k15,k8,k10,k4,k10,k9,k13,k11,k17,k9,k15,k15,k13,k1
num=1 count=1 "k15"
num=2 count=2 ["k12","k15"]
num=10 count=10 ["k1","k3","k6","k7","k8","k11","k12","k15","k18","k19"]
num=19 count=19 ["k0","k1","k2","k3","k4","k5","k6","k7","k8","k9","k10","k11","k12","k13","k14","k16","k17","k18","k19"]
num=20 count=20 ["k0","k1","k2","k3","k4","k5","k6","k7","k8","k9","k10","k11","k12","k13","k14","k15","k16","k17","k18","k19"]
int(1)
int(0)
int(5)
