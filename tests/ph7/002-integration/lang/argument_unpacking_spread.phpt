--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Argument unpacking adjusts call arg count correctly
--FILE--
<?php
// Spread into fixed params
function add3($a, $b, $c) {
    return $a + $b + $c;
}
$nums = [10, 20, 30];
echo add3(...$nums) . "\n";

// Mixed regular + spread
function mixed($a, $b, $c, $d) {
    return "$a-$b-$c-$d";
}
$rest = [3, 4];
echo mixed(1, 2, ...$rest) . "\n";

// Spread empty array (no args contributed)
function optionalAll(...$items) {
    return count($items);
}
$empty = [];
echo optionalAll(...$empty) . "\n";

// Spread into variadic
function collectAll(...$items) {
    return implode(",", $items);
}
$more = [10, 20, 30];
echo collectAll(...$more) . "\n";
?>
--EXPECT--
60
1-2-3-4
0
10,20,30
--CLEAN--
<?php

