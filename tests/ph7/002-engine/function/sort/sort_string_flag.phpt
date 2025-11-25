--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sort with SORT_STRING flag should compare values as strings
--SKIPIF--
<?php
if (function_exists('zend_version')) {
	echo "skip"; // Zend/PHP flags semantics differ from PH7
}
?>
--FILE--
<?php
$a = array('2', '10', '1');
sort($a, SORT_STRING);
foreach($a as $v) echo $v . PHP_EOL;
?>
--EXPECT--
1
2
10
--CLEAN--
<?php
unset($a);
?>
