--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Array insertion by reference and foreign object handling
--FILE--
<?php
// Test array operations with references
$var = 10;
$a = array(&$var);
echo count($a) . ' ';
$var = 20;
echo $a[0] . ' ';
$a[0] = 30;
echo $var . PHP_EOL;

// Test overwriting reference
$a = array();
$a[] =& $var;
echo count($a) . ' ';
unset($var);
echo count($a) . PHP_EOL;
?>
--EXPECT--
1 20 30
1 0
--CLEAN--
<?php
unset($var, $a);
