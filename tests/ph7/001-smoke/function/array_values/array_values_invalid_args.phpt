--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array_values with no arguments - should return NULL
$result = array_values();
var_dump($result);
?>
--EXPECT--
null
--CLEAN--
<?php
unset($result);
