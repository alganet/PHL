--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex string interpolation with deep nesting
--FILE--
<?php
// Test complex string interpolation scenarios
// Covers GenStateProcessStringExpression additional paths (lines ~492-494, 560-568, 640-641)

$a = "world";
$b = "hello";
$array = array("value");

$test1 = "Hello {$a} and {$b}";
$test2 = "Array: {$array[0]}";
$test3 = "Nested: {$b} {$a}";

echo $test1 . "\n";
echo $test2 . "\n";
echo $test3 . "\n";

echo "Done\n";
?>
--EXPECT--
Hello world and hello
Array: value
Nested: hello world
Done
--CLEAN--
<?php
unset($a, $b, $array, $test1, $test2, $test3);
