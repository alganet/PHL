--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_shift should remove the first element and return it, updating the array
--FILE--
<?php
$a = array('first','second','third');
$val = array_shift($a);
echo $val . PHP_EOL; // first
echo implode(',', $a) . PHP_EOL; // second,third
// on empty, shift returns null
$b = array();
$val = array_shift($b);
echo (is_null($val) ? 'NULL' : $val) . PHP_EOL;
?>
--EXPECT--
first
second,third
NULL
--CLEAN--
<?php
unset($a, $val, $b);
