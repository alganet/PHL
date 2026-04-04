--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullable parameter type hints with ? prefix
--FILE--
<?php
function greet(?string $name): string {
    if ($name === null) {
        return "Hello, anonymous";
    }
    return "Hello, $name";
}
echo greet("Alice") . "\n";
echo greet(null) . "\n";

function addMaybe(?int $a, ?int $b): int {
    return ($a ?? 0) + ($b ?? 0);
}
echo addMaybe(3, 4) . "\n";
echo addMaybe(null, 5) . "\n";
echo addMaybe(null, null) . "\n";
?>
--EXPECT--
Hello, Alice
Hello, anonymous
7
5
0
--CLEAN--
<?php

