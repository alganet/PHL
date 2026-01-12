--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with no arguments
$result = array_sum();
echo "no_args: $result\n";

// Test with string argument
$result = array_sum("not an array");
echo "string_arg: $result\n";

// Test with integer argument
$result = array_sum(42);
echo "int_arg: $result\n";
?>
--EXPECT--
no_args: 0
string_arg: 0
int_arg: 0