--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_erase should clear the content of the array
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
$a = array('x' => 1, 'y' => 2);
array_erase($a);
echo count($a) . PHP_EOL; // 0
?>
--EXPECT--
0
--CLEAN--
<?php
unset($a,$res);
?>
