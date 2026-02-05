--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal compilation and operations
--FILE--
<?php
// Test integer literals
$i1 = 42;
$i2 = -123;
$i3 = 0xFF;  // hexadecimal
$i4 = 077;   // octal
$i5 = 0b101; // binary

// Test float literals
$f1 = 3.14;
$f2 = -2.5;
$f3 = 1.23e4;
$f4 = 5.67E-2;

echo $i1 + $f1; echo "\n";
echo $i3 * $f2; echo "\n";
echo $f1 * $f2; echo "\n";
?>
--EXPECT--
45.14
-637.5
-7.85
--CLEAN--
<?php
unset($i1, $i2, $i3, $i4, $i5, $f1, $f2, $f3, $f4);
