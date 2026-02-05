--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array_key_exists with no arguments - should return FALSE
$result = array_key_exists();
var_dump($result);

// Test array_key_exists with only one argument - should return FALSE
$result2 = array_key_exists('key');
var_dump($result2);
?>
--EXPECT--
bool(FALSE)
bool(FALSE)
--CLEAN--
<?php
unset($result, $result2);
