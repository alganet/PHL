--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in function arguments and return values
--FILE--
<?php
function withDefault($x = 1_000) {
    return $x;
}
echo withDefault() . "\n";
echo withDefault(2_500) . "\n";

function double($n) {
    return $n * 2;
}
echo double(1_000) . "\n";
echo double(0xFF_FF) . "\n";

function returnsLarge() {
    return 1_000_000;
}
echo returnsLarge() . "\n";

function sumThree($a, $b, $c) {
    return $a + $b + $c;
}
echo sumThree(1_000, 2_000, 3_000) . "\n";
?>
--EXPECT--
1000
2500
2000
131070
1000000
6000
--CLEAN--
<?php

