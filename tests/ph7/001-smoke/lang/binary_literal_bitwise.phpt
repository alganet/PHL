--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal bitwise operations
--FILE--
<?php
echo (0b1100 & 0b1010) . "\n";
echo (0b1100 | 0b1010) . "\n";
echo (0b1100 ^ 0b1010) . "\n";
echo (~0b0 & 0xFF) . "\n";
echo (0b1 << 0b100) . "\n";
echo (0b10000 >> 0b10) . "\n";
?>
--EXPECT--
8
14
6
255
16
4
--CLEAN--
<?php

