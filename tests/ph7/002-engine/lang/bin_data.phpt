--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary number parsing (0b prefix)
--FILE--
<?php
// Test binary number literals with 0b prefix
echo "Testing binary number parsing:\n";

// Test basic binary numbers
echo "0b0 = " . (0b0) . "\n";
echo "0b1 = " . (0b1) . "\n";
echo "0b1010 = " . (0b1010) . "\n";
echo "0b11111111 = " . (0b11111111) . "\n";

// Test binary numbers in expressions
echo "0b1010 + 0b0101 = " . (0b1010 + 0b0101) . "\n";
echo "0b1 * 8 = " . (0b1 * 8) . "\n";

// Test case insensitive
echo "0B1010 = " . (0B1010) . "\n";
?>
--EXPECT--
Testing binary number parsing:
0b0 = 0
0b1 = 1
0b1010 = 10
0b11111111 = 255
0b1010 + 0b0101 = 15
0b1 * 8 = 8
0B1010 = 10