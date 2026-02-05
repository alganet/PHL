--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Insert by reference should ensure that when the foreign variable is unset, the entry disappears from the array
--SKIPIF--
<?php
if (function_exists('zend_version')) {
	echo "skip"; /* Behavior differs on Zend; PH7 provides insertion by reference semantics */
}
?>
--FILE--
<?php
$var = 10;
$a = array();
$a[] =& $var; // Insert by reference
echo count($a) . PHP_EOL; // should be 1
unset($var);
// The foreign object was unset, the array entry should be removed
echo count($a) . PHP_EOL; // should be 0
?>
--EXPECT--
1
0
--CLEAN--
<?php
unset($var, $a);
