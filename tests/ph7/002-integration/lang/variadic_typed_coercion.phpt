--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed variadic parameters apply per-element type coercion
--FILE--
<?php
function sumInts(int ...$nums): int {
    $total = 0;
    foreach ($nums as $n) {
        $total += $n;
    }
    return $total;
}
// String args should be coerced to int
echo sumInts("3", "4", "5") . "\n";

function joinStrings(string ...$parts): string {
    return implode("-", $parts);
}
// Int args should be coerced to string
echo joinStrings(1, 2, 3) . "\n";
?>
--EXPECT--
12
1-2-3
--CLEAN--
<?php

