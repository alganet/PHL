--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal with printf/sprintf formatting
--FILE--
<?php
printf("%d\n", 0b1010);
printf("%b\n", 0b1010);
printf("%o\n", 0b1010);
printf("%x\n", 0b1010);
printf("%X\n", 0b1010);

echo sprintf("%08b", 0b1010) . "\n";
echo sprintf("%08b", 0b11111111) . "\n";

echo decbin(0b1010) . "\n";
echo bindec("1010") === 0b1010 ? "true" : "false";
echo "\n";
?>
--EXPECT--
10
1010
12
a
A
00001010
11111111
1010
true
--CLEAN--
<?php

