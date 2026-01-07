--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test complex expression parsing to cover uncovered compilation paths
--FILE--
<?php
// Test complex expressions that may exercise uncovered paths in PH7_CompileExpr
// and related expression compilation functions

// Complex arithmetic with mixed types
$result = (5 + 3.14) * 2 / (1 + 1);
echo "Complex math: " . $result . "\n";

// Nested function calls and expressions
$value = strlen(trim("  test string  ")) + abs(-10);
echo "Function nesting: " . $value . "\n";

// Complex array operations
$array = array(1, 2, array(3, 4, 5));
$nested = $array[2][1] + $array[0];
echo "Array nesting: " . $nested . "\n";

// String concatenation with variables
$text = "Hello" . " " . "World" . " " . 2025;
echo "String concat: " . $text . "\n";

// Complex conditional expressions
$flag = ($result > 10) && ($value < 20) || (count($array) == 3);
echo "Complex condition: " . ($flag ? "true" : "false") . "\n";
?>
--EXPECT--
Complex math: 8.14
Function nesting: 21
Array nesting: 5
String concat: Hello World 2025
Complex condition: true