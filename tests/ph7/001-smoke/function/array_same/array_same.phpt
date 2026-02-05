--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_same should return TRUE if two variables reference the same array
--SKIPIF--
<?php if(function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
$a = array(1,2);
$b = $a; // In PH7 this keeps the same instance
$c = array(1,2);
echo array_same($a, $b) ? "ok\n" : "fail\n";
echo array_same($a, $c) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
fail
--CLEAN--
<?php
unset($a, $b, $c);
