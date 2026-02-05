--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with insufficient arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test base_convert with less than 3 arguments
$result = base_convert("1010", 2);
var_dump($result);
$result2 = base_convert("1010");
var_dump($result2);
$result3 = base_convert();
var_dump($result3);
?>
--EXPECT--
string(0 '')
string(0 '')
string(0 '')
--CLEAN--
<?php
unset($result, $result2, $result3);
