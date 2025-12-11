--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_copy should return a deep copy that is independent from the original
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
$a = array('x' => 1, 'y' => 2);
$b = array_copy($a);
$b['x'] = 42;
echo $a['x'] . PHP_EOL; // 1
echo $b['x'] . PHP_EOL; // 42
?>
--EXPECT--
1
42
--CLEAN--
<?php
unset($a,$b);
?>
