--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal in variable assignment and expressions
--FILE--
<?php
$a = 0b1010;
echo $a . "\n";

$b = 0b1010 << 4;
echo $b . "\n";

$c = $a + 0b0101;
echo $c . "\n";

$a += 0b1;
echo $a . "\n";

$a -= 0b10;
echo $a . "\n";

$a *= 0b10;
echo $a . "\n";

$a &= 0b1111;
echo $a . "\n";

$a |= 0b10000;
echo $a . "\n";

$a ^= 0b11111;
echo $a . "\n";

$a >>= 0b1;
echo $a . "\n";

$a <<= 0b10;
echo $a . "\n";
?>
--EXPECT--
10
160
15
11
9
18
2
18
13
6
24
--CLEAN--
<?php

