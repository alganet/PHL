--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Test comma operator expressions
--FILE--
<?php
// Test comma operator expressions to cover lowest precedence operators
$a = 1;
$result = ($a = 5, $a + 10);
var_dump($result); // Should be 15

$b = 2;
$c = ($b *= 3, $b + 2);
var_dump($c); // Should be 8

$d = 10;
$e = ($d++, $d * 2);
var_dump($e); // Should be 22
?>
--EXPECT--
int(15)
int(8)
int(22)
--CLEAN--
<?php
unset($a, $result, $b, $c, $d, $e);
