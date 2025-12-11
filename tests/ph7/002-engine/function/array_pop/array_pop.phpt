--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pop should remove the last element and return it, updating the original array
--FILE--
<?php
$a = array('x','y','z');
$val = array_pop($a);
echo $val . PHP_EOL; // z
echo implode(',', $a) . PHP_EOL; // x,y
// pop empty returns null
$empty = array();
// Some engines might not print NULL representation for var_export; use is_null guard instead
$val = array_pop($empty);
echo (is_null($val) ? 'NULL' : $val) . PHP_EOL;
?>
--EXPECT--
z
x,y
NULL
--CLEAN--
<?php
unset($a,$val,$empty);
?>
