--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test array iterator functions: current(), next(), prev(), end(), reset()
--FILE--
<?php
$arr = array('a' => 1, 'b' => 2, 'c' => 3);
// Start at first element
echo current($arr) . PHP_EOL; // 1
// Advance
echo next($arr) . PHP_EOL; // 2
// Current again
echo current($arr) . PHP_EOL; // 2
// Jump to end
echo end($arr) . PHP_EOL; // 3
// Back one
echo prev($arr) . PHP_EOL; // 2
// Reset to start
reset($arr);
echo current($arr) . PHP_EOL; // 1
?>
--EXPECT--
1
2
2
3
2
1
--CLEAN--
<?php
unset($arr);
?>
