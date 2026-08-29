--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Multiple unary operators in expression

--FILE--
<?php
// Test multiple unary operators
$value = 5;
$result = + - + $value;
echo "Result: $result\n";

// Test with negation and increment
$value2 = 10;
$result2 = - + ++$value2;
echo "Result2: $result2\n";
echo "Value2: $value2\n";
?>
--EXPECT--
Result: -5
Result2: -11
Value2: 11
--CLEAN--
<?php
unset($value, $result, $value2, $result2);
