--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf handles complex format specifiers
--FILE--
<?php
// Test various format specifiers that might exercise different code paths
$result1 = sprintf("%-10s", "test");  // Left justify
$result2 = sprintf("%x", 255);       // Hex format
$result3 = sprintf("%+d", 42);       // Force sign
$result4 = sprintf("% d", -42);      // Space for positive
$result5 = sprintf("%010d", 123);    // Zero padding

echo $result1 . "\n";
echo $result2 . "\n";
echo $result3 . "\n";
echo $result4 . "\n";
echo $result5 . "\n";
?>
--EXPECT--
test      
ff
+42
-42
0000000123