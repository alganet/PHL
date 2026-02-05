--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array_keys with no arguments - should return NULL
$result = array_keys();
var_dump($result);
?>
--EXPECT--
null
--CLEAN--
<?php
unset($result);
