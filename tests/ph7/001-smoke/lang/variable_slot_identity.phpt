--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A variable name still means the right slot after every rebinding route
--FILE--
<?php
/* A variable's slot is found by NUMBER, not by name. Every route that
 * can move a NAME to another slot has to be seen by the frame that numbered it. */

/* unset() then re-create: the name means a different slot the second time. */
function vslot_unset() {
    $a = 'first';
    $seen = $a;
    unset($a);
    $a = 'second';
    return $seen . '/' . $a;
}
echo vslot_unset(), "\n";

/* `=&` on a name that already exists REBINDS it. */
function vslot_rebind() {
    $x = 1;
    $y = 2;
    $before = $y;
    $y =& $x;
    $y = 9;
    return $before . '/' . $x . '/' . $y;
}
echo vslot_rebind(), "\n";

/* `=&` on a name the frame does not have yet is an INSERT, and the frame may
 * still remember where that name lived. */
function vslot_insert_ref() {
    $src = 5;
    $seen = isset($alias) ? 'set' : 'unset';
    $alias =& $src;
    $alias = 6;
    return $seen . '/' . $src . '/' . $alias;
}
echo vslot_insert_ref(), "\n";

/* `global` re-points a local name at the global slot. */
$vslot_g = 'global';
function vslot_global() {
    $vslot_g = 'local';
    $before = $vslot_g;
    global $vslot_g;
    $after = $vslot_g;
    $vslot_g = 'written';
    return $before . '/' . $after;
}
echo vslot_global(), " ", $vslot_g, "\n";

/* extract() installs names the body also names literally. */
function vslot_extract() {
    $e = 'own';
    $before = $e;
    extract(['e' => 'extracted']);
    return $before . '/' . $e;
}
echo vslot_extract(), "\n";

/* static binds the name to a slot that outlives the activation. */
function vslot_static() {
    static $n;
    $n = ($n ?? 0) + 1;
    $local = $n * 10;
    return $local;
}
echo vslot_static(), ',', vslot_static(), ',', vslot_static(), "\n";

/* A by-reference foreach re-binds its value variable on every step, and php
 * leaves it bound to the last element afterwards. */
function vslot_foreach_ref() {
    $a = [1, 2, 3];
    foreach ($a as &$v) {
        $v *= 2;
    }
    $held = $v;
    foreach ($a as $v) {
    }
    return implode(',', $a) . '/' . $held;
}
echo vslot_foreach_ref(), "\n";

/* A body with more distinct names than the frame can number: the ones that do
 * not fit take the hash path, and the ones that do must still see a rebind. */
function vslot_over_cap() {
    $n01=1;  $n02=2;  $n03=3;  $n04=4;  $n05=5;  $n06=6;  $n07=7;  $n08=8;
    $n09=9;  $n10=10; $n11=11; $n12=12; $n13=13; $n14=14; $n15=15; $n16=16;
    $n17=17; $n18=18; $n19=19; $n20=20; $n21=21; $n22=22; $n23=23; $n24=24;
    $n25=25; $n26=26; $n27=27; $n28=28; $n29=29; $n30=30; $n31=31; $n32=32;
    $sum = 0;
    for ($i = 0; $i < 3; $i++) {
        $sum += $n01 + $n16 + $n32;
    }
    unset($n01, $n32);
    $n01 = 100;
    $n32 = 200;
    return $sum . '/' . ($n01 + $n32) . '/' . $n16;
}
echo vslot_over_cap(), "\n";

/* A closure by-reference capture writes through the enclosing name. */
function vslot_closure() {
    $c = 0;
    $bump = function () use (&$c) { $c++; };
    $bump();
    $bump();
    return $c;
}
echo vslot_closure(), "\n";

/* A generator's locals survive every suspension. */
function vslot_gen() {
    $i = 0;
    $acc = 0;
    while ($i < 4) {
        $acc += $i;
        yield $acc;
        $i++;
    }
}
$vslot_out = [];
foreach (vslot_gen() as $vslot_step) {
    $vslot_out[] = $vslot_step;
}
echo implode(',', $vslot_out), "\n";

/* A catch body shares the frame with the try it is written in. */
function vslot_catch() {
    $state = 'try';
    try {
        throw new RuntimeException('boom');
    } catch (RuntimeException $e) {
        $state = 'catch';
        $msg = $e->getMessage();
    }
    return $state . '/' . $msg;
}
echo vslot_catch(), "\n";

/* A dynamic name reaches the same storage as the literal one. */
function vslot_dynamic() {
    $d = 1;
    $name = 'd';
    $$name = 2;
    return $d . '/' . $$name;
}
echo vslot_dynamic(), "\n";

/* Recursion: every activation answers for its own slots. */
function vslot_recurse($depth) {
    if ($depth === 0) {
        return '';
    }
    $here = $depth;
    $rest = vslot_recurse($depth - 1);
    return $here . $rest;
}
echo vslot_recurse(6), "\n";
--EXPECT--
first/second
2/9/9
unset/6/6
local/global written
own/extracted
10,20,30
2,4,4/6
147/300/16
2
0,1,3,6
catch/boom
2/2
654321
