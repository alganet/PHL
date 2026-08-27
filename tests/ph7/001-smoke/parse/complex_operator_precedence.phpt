--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test complex operator precedence and expression parsing
--FILE--
<?php
// Test complex expressions mixing multiple operators
$a = 5;
$b = 3;
$c = 2;

// Test arithmetic with assignment and comparison
$result1 = ($a + $b) * $c > 10 and $a - $b == 2;
var_dump($result1); // bool(true)

// Test ternary with arithmetic
$result2 = $a > $b ? $a + $c : $b - $c;
var_dump($result2); // int(7)

// Test string concatenation with arithmetic
$result3 = "Value: " . ($a * $b + $c);
var_dump($result3); // string(9) "Value: 17"

// Test array access with arithmetic
$arr = array(10, 20, 30);
$result4 = $arr[$a - $b - 2] + $c;
var_dump($result4); // int(12)

// Test function call with complex expression
function copTestFunc($x) {
    return $x * 2;
}
$result5 = copTestFunc($a + $b) - $c;
var_dump($result5); // int(14)

// Test modulus and division precedence
$result6 = 10 + 5 % 3 * 2 - 1;
var_dump($result6); // int(13) - (10 + ((5 % 3) * 2) - 1)
?>
--EXPECT--
bool(true)
int(7)
string(9) "Value: 17"
int(12)
int(14)
int(13)
--CLEAN--
<?php
unset($a, $b, $c, $result1, $result2, $result3, $arr, $result4, $result5, $result6);
