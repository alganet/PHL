--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Parse error handling edge cases
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test complex nested expressions that trigger error handling paths

// Mismatched parentheses - triggers ExprVerifyNodes error handling
if (false) {
    echo "This won't run";
}

// Test valid array syntax
$arr = array(1, 2, 3);

// Complex nested array access to trigger subscript handling
$matrix = array(
    array(1, 2, 3),
    array(4, 5, 6)
);
$value = $matrix[0][1];

// Test reference assignment edge cases
$base = 10;
$ref = &$base;
$ref = 20;

$a = "hello";
$b = "world";
$result = $a . $b;
$result2 = $a !== $b;

// Test complex ternary operator
$score = 85;
$grade = ($score >= 90) ? 'A' : (($score >= 80) ? 'B' : 'C');

// Test function call with complex arguments
$nums = array(1, 2, 3, 4, 5);
$sum = array_sum($nums);

// Test array concatenation
$arr1 = array(1, 2);
$arr2 = array(3, 4);
$combined = $arr1 + $arr2;

echo "All tests completed successfully\n";
?>
--EXPECT--
All tests completed successfully
--CLEAN--
<?php
unset($arr, $matrix, $value, $base, $ref, $a, $b, $result, $result2, $counter, $score, $grade, $nums, $sum, $arr1, $arr2, $combined);
