--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf() error handling and edge cases
--FILE--
<?php
// Test various edge cases to trigger error handling paths
echo sprintf("Simple: %s", "test") . "\n";
echo sprintf("Number: %d", 42) . "\n";
echo sprintf("Float: %.2f", 3.14) . "\n";
echo sprintf("Hex: %x", 255) . "\n";
echo sprintf("Octal: %o", 8) . "\n";
?>
--EXPECT--
Simple: test
Number: 42
Float: 3.14
Hex: ff
Octal: 10