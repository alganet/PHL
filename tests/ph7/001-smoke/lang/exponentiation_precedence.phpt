--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exponentiation precedence vs *, /, %, +, unary, cast, parens
--FILE--
<?php
// ** tighter than * / %
echo 2 * 3 ** 2, "\n";       // 2 * 9 = 18
echo 3 ** 2 * 2, "\n";       // 9 * 2 = 18
echo 2 ** 3 / 2, "\n";       // 8 / 2 = 4
echo 2 ** 3 % 5, "\n";       // 8 % 5 = 3

// ** tighter than +
echo 2 ** 3 + 1, "\n";       // 8 + 1 = 9
echo 2 ** (3 + 1), "\n";     // 2 ** 4 = 16

// ** right-associative
echo 2 ** 3 ** 2, "\n";      // 2 ** (3 ** 2) = 512
echo 4 ** 2 ** 2, "\n";      // 4 ** 4 = 256
echo 2 ** -3 ** 2, "\n";     // 2 ** (-(3**2)) = 2 ** -9

// ** binds tighter than unary -, +, ~, (cast)
echo -2 ** 2, "\n";          // -(2**2) = -4
echo -3 ** 3, "\n";          // -(3**3) = -27
echo ~2 ** 2, "\n";          // ~(2**2) = ~4 = -5
echo (int) 2.5 ** 2, "\n";   // (int)(2.5**2) = (int)6.25 = 6
echo (float) 2 ** 3, "\n";   // (float)(2**3) = 8

// Nested unary preserves order: - -2 ** 2 = -(-(2**2)) = -(-4) = 4
echo - -2 ** 2, "\n";
echo ~-2 ** 2, "\n";         // ~(-(2**2)) = ~(-4) = 3

// Parens override the unary hoist
echo (-2) ** 2, "\n";        // 4
echo (-2) ** 3, "\n";        // -8
echo -(-2) ** 3, "\n";       // -((-2)**3) = -(-8) = 8

// ! has lower precedence than ** in PHP — hoist still gives the right answer
echo !2 ** 2 ? "T" : "F", "\n";  // !(2**2) = !4 = false

// Error-suppression '@' acts as a passthrough unary: ** still binds tighter
echo @-2 ** 2, "\n";         // @(-(2**2)) = @(-4) = -4
echo @(-2) ** 2, "\n";       // @((-2)**2) = @(4) = 4  (parens block hoist)
echo -@2 ** 2, "\n";         // -(@(2**2)) = -(@(4)) = -4

// Negative exponent yields float
echo 2 ** -2, "\n";          // 0.25

// Interplay with variables and compound assign
$a = 2;
$a = 3 ** $a;
echo $a, "\n";               // 9
$b = 5;
$b **= 2;
echo $b, "\n";               // 25

// Non-commutative **= (operand order sanity)
$c = 3; $c **= 2; echo $c, "\n";   // 9
$d = 2; $d **= 3; echo $d, "\n";   // 8

// String operands are coerced to int
echo "2" ** "10", "\n";      // 1024
?>
--EXPECT--
18
18
4
3
9
16
512
256
0.001953125
-4
-27
-5
6
8
4
3
4
-8
8
F
-4
4
-4
0.25
9
25
9
8
1024
