--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Variadic function parameters with ...
--FILE--
<?php
function sum(...$nums) {
    $total = 0;
    foreach ($nums as $n) {
        $total += $n;
    }
    return $total;
}
echo sum(1, 2, 3, 4, 5) . "\n";
echo sum() . "\n";

function greet(string $greeting, string ...$names) {
    echo $greeting . " " . implode(", ", $names) . "\n";
}
greet("Hello", "Alice", "Bob");

function countArgs(...$args) {
    return count($args);
}
echo countArgs(1, 2, 3) . "\n";
echo countArgs() . "\n";
?>
--EXPECT--
15
0
Hello Alice, Bob
3
0
--CLEAN--
<?php

