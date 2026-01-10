--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$GLOBALS is a read-only array and insertion with int key should emit a notice and not add the key
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip"; // Behavior differs on Zend
}
?>
--FILE--
<?php
$restore = set_error_handler(function($errno, $errstr, $errfile, $errline){
    echo $errstr . PHP_EOL;
    return true; // Suppress engine default error output
});
$GLOBALS[0] = 'value';
// Insertion should be forbidden
echo isset($GLOBALS[0]) ? '1' . PHP_EOL : '0' . PHP_EOL;
set_error_handler($restore);
?>
--EXPECT--
$GLOBALS is a read-only array,insertion is forbidden
0
--CLEAN--
<?php
unset($GLOBALS[0]);
?>