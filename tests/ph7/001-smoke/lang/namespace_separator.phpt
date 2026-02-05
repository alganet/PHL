--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace separator operator testing (PHL compatible)
--FILE--
<?php
// Test namespace separator (\) edge cases
// Note: PHL has namespace support disabled
// namespace\test\constant;
// echo "namespace separator test\n";

// Test with backslash in string (should not be namespace separator)
echo "backslash in string: \\test\n";

// Test class instantiation with our own simple class
class TestClass {}
$obj = new TestClass();
echo get_class($obj) . "\n";

// Test other lexer features that might be uncovered
echo "Testing other lexer features:\n";

// Test various operators
$a = 5; $b = 3;
echo ($a & $b) . "\n";  // Bitwise AND
echo ($a | $b) . "\n";  // Bitwise OR
echo ($a ^ $b) . "\n";  // Bitwise XOR

// Test assignment operators
$a = 16; $a <<= 2; echo $a . "\n";  // Left shift assign
$b = 64; $b >>= 3; echo $b . "\n";  // Right shift assign

echo "Lexer test completed\n";
?>
--EXPECT--
backslash in string: \test
TestClass
Testing other lexer features:
1
7
6
64
8
Lexer test completed
--CLEAN--
<?php
unset($obj, $a, $b);
