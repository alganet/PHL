--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 always assign by reference
--SKIPIF--
<?php
if (function_exists('zend_version')) {
	echo "skip"; // PHP/Zend semantics differ from PH7 here
}
?>
--FILE--
<?php
$a = array('x' => 1);
$b = $a; // Duplicate
$b['x'] = 2;
echo $a['x'] . PHP_EOL;
echo $b['x'] . PHP_EOL;
?>
--EXPECT--
2
2
--CLEAN--
<?php
unset($a,$b);
?>
