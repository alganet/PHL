--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array union operator '+' should preserve left-side values on key collision and append new keys from right side
--FILE--
<?php
$a = array('a' => 'apple', 'b' => 'banana');
$b = array('a' => 'pear', 'b' => 'strawberry', 'c' => 'cherry');
$c = $a + $b;
echo count($c) . PHP_EOL; // 3
echo $c['a'] . PHP_EOL; // 'apple' (left wins)
echo $c['c'] . PHP_EOL; // 'cherry' (from right)
?>
--EXPECT--
3
apple
cherry
--CLEAN--
<?php
unset($a, $b, $c);
?>
