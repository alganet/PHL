--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unary minus ambiguity - test expression starting with + or - operator
--FILE--
<?php
// Test unary minus/plus detection in expressions starting with + or -
// This should trigger the unary vs binary operator detection logic
$result = -5;
$result2 = +10;
echo "Result: $result\n";
echo "Result2: $result2\n";
?>
--EXPECT--
Result: -5
Result2: 10
--CLEAN--
<?php
unset($result, $result2);
