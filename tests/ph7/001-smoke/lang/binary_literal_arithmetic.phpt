--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal arithmetic operations
--FILE--
<?php
echo 0b1010 + 0b0101 . "\n";
echo 0b1010 - 0b11 . "\n";
echo 0b1010 * 2 . "\n";
echo 0b10100 / 0b10 . "\n";
echo 0b1010 % 0b11 . "\n";
echo pow(0b10, 0b1010) . "\n";
echo -0b1010 . "\n";
echo +0b1010 . "\n";
?>
--EXPECT--
15
7
20
10
1
1024
-10
10
--CLEAN--
<?php

