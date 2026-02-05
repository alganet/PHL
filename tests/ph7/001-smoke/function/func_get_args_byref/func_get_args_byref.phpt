--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
func_get_args_byref returns arguments by reference
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
function inc(&$v){ $v += 1; }
function wrapper(&$a){
    $args = func_get_args_byref();
    $args[0] += 1; // modify via reference
}

$a = 10;
wrapper($a);
echo $a . "\n";
?>
--EXPECT--
11
--CLEAN--
<?php
unset($args, $a);
