--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Complex expression precedence with nested operations
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test complex expression precedence with nested ternary and binary operations
$a = 5;
$b = 10;
$c = 15;

// Test precedence of ternary with arithmetic and comparison
$result = $a + $b * 2 > $c ? $a + $b : $c - $a;
var_dump($result); // Should be 15 (25 > 15 is true, so 15)

// Test nested operations
$d = ($a < $b ? $a : $b) + ($c > $b ? $c : $b);
var_dump($d); // Should be 20 (5 + 15)
?>
--EXPECT--
int(15)
int(20)
--CLEAN--
<?php
unset($a, $b, $c, $result, $d);
