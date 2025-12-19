--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array_intersect_key with integer keys
--FILE--
<?php
$array1 = array(10 => 'a', 20 => 'b', 30 => 'c');
$array2 = array(10 => 'x', 40 => 'y');
$result = array_intersect_key($array1, $array2);
var_dump(count($result));
var_dump(isset($result[10]));
var_dump(isset($result[20]));
?>
--EXPECT--
int(1)
bool(TRUE)
bool(FALSE)