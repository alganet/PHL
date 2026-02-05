--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Object type casting for scalar values
--FILE--
<?php
// Test casting scalar values to object

// String to object
$str = "hello";
$obj_str = (object)$str;
echo "string to object: " . (is_object($obj_str) && isset($obj_str->value) && $obj_str->value === "hello" ? 'ok' : 'fail') . "\n";

// Int to object
$int = 42;
$obj_int = (object)$int;
echo "int to object: " . (is_object($obj_int) && isset($obj_int->value) && $obj_int->value === 42 ? 'ok' : 'fail') . "\n";

// Float to object
$float = 3.14;
$obj_float = (object)$float;
echo "float to object: " . (is_object($obj_float) && isset($obj_float->value) && $obj_float->value == 3.14 ? 'ok' : 'fail') . "\n";

// Bool to object
$bool = true;
$obj_bool = (object)$bool;
echo "bool to object: " . (is_object($obj_bool) && isset($obj_bool->value) && $obj_bool->value === true ? 'ok' : 'fail') . "\n";

?>
--EXPECT--
string to object: ok
int to object: ok
float to object: ok
bool to object: ok
--CLEAN--
<?php
unset($str, $obj_str, $int, $obj_int, $float, $obj_float, $bool, $obj_bool);
