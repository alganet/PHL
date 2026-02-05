--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unary plus and minus operators at expression start
--FILE--
<?php
// Test unary plus
$a = +5;
echo "Unary plus: " . $a . "\n";

// Test unary minus
$b = -5;
echo "Unary minus: " . $b . "\n";

// Test unary plus with variable
$c = 10;
$d = +$c;
echo "Unary plus with variable: " . $d . "\n";

// Test unary minus with variable
$e = -$c;
echo "Unary minus with variable: " . $e . "\n";

// Test in expressions
$f = +3 * 2;
echo "Unary plus in expression: " . $f . "\n";

$g = -3 + 5;
echo "Unary minus in expression: " . $g . "\n";
?>
--EXPECT--
Unary plus: 5
Unary minus: -5
Unary plus with variable: 10
Unary minus with variable: -10
Unary plus in expression: 6
Unary minus in expression: 2
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $f, $g);
