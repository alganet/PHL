--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object type casting for scalar values: a single "scalar" property (PHP-exact)
--FILE--
<?php
// (object)$scalar produces a stdClass with one property named "scalar".
$obj_str = (object)"hello";
echo "string to object: " . (is_object($obj_str) && isset($obj_str->scalar) && $obj_str->scalar === "hello" ? 'ok' : 'fail') . "\n";

$obj_int = (object)42;
echo "int to object: " . (is_object($obj_int) && isset($obj_int->scalar) && $obj_int->scalar === 42 ? 'ok' : 'fail') . "\n";

$obj_float = (object)3.14;
echo "float to object: " . (is_object($obj_float) && isset($obj_float->scalar) && $obj_float->scalar === 3.14 ? 'ok' : 'fail') . "\n";

$obj_bool = (object)true;
echo "bool to object: " . (is_object($obj_bool) && isset($obj_bool->scalar) && $obj_bool->scalar === true ? 'ok' : 'fail') . "\n";

// (object)null produces an empty stdClass (no properties).
$obj_null = (object)null;
echo "null to object: " . (is_object($obj_null) && count(get_object_vars($obj_null)) === 0 ? 'ok' : 'fail') . "\n";
?>
--EXPECT--
string to object: ok
int to object: ok
float to object: ok
bool to object: ok
null to object: ok
--CLEAN--
<?php
unset($obj_str, $obj_int, $obj_float, $obj_bool, $obj_null);
