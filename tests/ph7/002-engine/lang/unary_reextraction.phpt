--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unary operator re-extraction in expressions to cover ExprVerifyNodes logic
--FILE--
<?php
// Test various unary + and - expressions to trigger re-extraction logic
$a = 5;

// Unary plus
$result1 = +$a;
echo "Unary plus: " . $result1 . "\n";

// Unary minus
$result2 = -$a;
echo "Unary minus: " . $result2 . "\n";

// Unary in expressions
$result3 = +$a + 10;
echo "Unary plus in expr: " . $result3 . "\n";

$result4 = -$a + 10;
echo "Unary minus in expr: " . $result4 . "\n";

// Multiple unary
$result5 = +(-$a);
echo "Nested unary: " . $result5 . "\n";

// Unary at start of statement
+$a;
-$a;
echo "Unary statements executed\n";
?>
--EXPECT--
Unary plus: 5
Unary minus: -5
Unary plus in expr: 15
Unary minus in expr: 5
Nested unary: -5
Unary statements executed