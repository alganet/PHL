--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test expression parsing edge cases
--FILE--
<?php
// Test various expression parsing scenarios to cover uncovered paths
$a = 5;
$b = 10;
$c = $a + $b;
echo "Result: " . $c . "\n";

// Test array operations
$arr = array(1, 2, 3);
echo "Array length: " . count($arr) . "\n";

// Test string operations
$str1 = "Hello";
$str2 = "World";
echo $str1 . " " . $str2 . "\n";
?>
--EXPECT--
Result: 15
Array length: 3
Hello World
--CLEAN--
<?php
unset($a, $b, $c, $arr, $str1, $str2);
