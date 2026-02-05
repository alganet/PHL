--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: str_replace with array containing empty string key
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test str_replace with array containing empty key
$result = str_replace(array('', 'l'), array('x', 'L'), 'hello');
var_dump($result);
?>
--EXPECT--
string(5 'hexxo')
--CLEAN--
<?php
unset($result);
