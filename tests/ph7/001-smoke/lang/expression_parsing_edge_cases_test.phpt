--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test expression parsing edge cases to cover uncovered parse.c lines
--FILE--
<?php
// Test various expression parsing scenarios to exercise parse.c code paths
$a = 5;
$b = 10;

// Test ternary operator
$result = $a > $b ? $a : $b;
echo "Ternary result: " . $result . "\n";

// Test array operations
$arr = array(1, 2, 3);
echo "Array length: " . count($arr) . "\n";

// Test function call
function expression_parsing_edge_cases_func($x, $y) {
    return $x + $y;
}
echo "Function call: " . expression_parsing_edge_cases_func(3, 4) . "\n";

// Test string concatenation
$str1 = "Hello";
$str2 = "World";
echo $str1 . " " . $str2 . "\n";
?>
--EXPECT--
Ternary result: 10
Array length: 3
Function call: 7
Hello World
--CLEAN--
<?php
unset($a, $b, $result, $arr, $str1, $str2);
