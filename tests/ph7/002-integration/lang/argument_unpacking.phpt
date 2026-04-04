--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Argument unpacking with ... in function calls
--FILE--
<?php
function sum($a, $b, $c) {
    return $a + $b + $c;
}
$args = [10, 20, 30];
echo sum(...$args) . "\n";

// Mixed: regular args + spread
function greet($greeting, $name1, $name2) {
    return "$greeting $name1 and $name2";
}
$names = ["Alice", "Bob"];
echo greet("Hello", ...$names) . "\n";

// Spread into variadic receiver
function all(...$items) {
    return implode(", ", $items);
}
$arr = [1, 2, 3];
echo all(...$arr) . "\n";
?>
--EXPECT--
60
Hello Alice and Bob
1, 2, 3
--CLEAN--
<?php

