--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in binary literals
--FILE--
<?php
echo 0b1010_1010 . "\n";
echo 0b1_1_1_1 . "\n";
echo 0b1111_0000 . "\n";
echo 0b1111_1111_1111_1111 . "\n";
echo 0B1010_0101 . "\n";
echo (int)(0b1010_1010 === 0b10101010) . "\n";
echo (int)(0b1_1_1_1 === 0b1111) . "\n";
echo (int)(0b1111_0000 === 0b11110000) . "\n";
?>
--EXPECT--
170
15
240
65535
165
1
1
1
--CLEAN--
<?php

