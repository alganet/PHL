--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal in function arguments and return values
--FILE--
<?php
function add($a, $b) {
    return $a + $b;
}

echo add(0b1010, 0b0101) . "\n";

function flags($mask) {
    $result = [];
    if ($mask & 0b0001) $result[] = "READ";
    if ($mask & 0b0010) $result[] = "WRITE";
    if ($mask & 0b0100) $result[] = "EXEC";
    return implode("|", $result);
}

echo flags(0b0001) . "\n";
echo flags(0b0011) . "\n";
echo flags(0b0111) . "\n";
echo flags(0b0100) . "\n";

function get_mask() {
    return 0b11001100;
}
echo get_mask() . "\n";
?>
--EXPECT--
15
READ
READ|WRITE
READ|WRITE|EXEC
EXEC
204
--CLEAN--
<?php

