--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff and array_diff_assoc should return expected keys and preserve keys across associative arrays
--FILE--
<?php
$a = array('a' => 1, 'b' => 2, 'c' => 3);
$b = array('b' => 2);
// array_diff only compares values
$c = array_diff($a, $b);
echo implode(',', array_keys($c)) . PHP_EOL; // expecting 'a,c'

// array_diff_assoc compares both key and value
$d = array_diff_assoc(array('a' => 1, 'b' => 2), array('a' => 1));
echo implode(',', array_keys($d)) . PHP_EOL; // expecting 'b'
?>
--EXPECT--
a,c
b
--CLEAN--
<?php
unset($a, $b, $c, $d);
