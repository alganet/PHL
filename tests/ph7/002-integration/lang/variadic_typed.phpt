--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Variadic parameters with type hints
--FILE--
<?php
function joinStrings(string ...$parts): string {
    return implode("-", $parts);
}
echo joinStrings("a", "b", "c") . "\n";

function sumInts(int ...$nums): int {
    $total = 0;
    foreach ($nums as $n) {
        $total += $n;
    }
    return $total;
}
echo sumInts(10, 20, 30) . "\n";
?>
--EXPECT--
a-b-c
60
--CLEAN--
<?php

