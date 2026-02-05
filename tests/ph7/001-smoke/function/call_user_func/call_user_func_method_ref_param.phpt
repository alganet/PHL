--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func_array with object method that takes a reference parameter
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
class C { public function inc(&$x){ $x += 2; } }
$c = new C();
$a = 3;
call_user_func_array(array($c,'inc'), array(&$a));
echo $a . "\n";
?>
--EXPECT--
5
--CLEAN--
<?php
unset($c, $a);
