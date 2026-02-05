--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex expression parsing
--FILE--
<?php
// Test complex expressions to cover parsing edge cases
$a = 5;
$b = 10;
$c = $a + $b * 2 - ($b / 2) % 3;
$d = ($c << 1) | ($c >> 2) & ~$c;
$e = $d ^ $c;
echo "Result: " . $e . "\n";

// Test with arrays
$arr = array(1, 2, 3);
$sum = $arr[0] + $arr[1] + $arr[2];
echo "Sum: " . $sum . "\n";

// Test string operations
$str = "Hello" . " " . "World";
echo "String: " . $str . "\n";
?>
--EXPECT--
Result: 57
Sum: 6
String: Hello World
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $arr, $sum, $str);
