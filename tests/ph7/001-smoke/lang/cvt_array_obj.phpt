--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Casts to array and object
--FILE--
<?php
// Cast scalar to array and assert is_array
$scalar = 42;
$arr = (array)$scalar;
echo is_array($arr) ? 'array' : 'not' ;
echo "\n";

// Build object and convert back to array to read properties
// Build object from array and assert types
$obj = (object)array('x' => 1, 'y' => 2);
echo is_object($obj) ? 'object' : 'not';
echo "\n";
$back_props = (array)$obj;
echo is_array($back_props) ? 'array' : 'not';
echo "\n";

$a = array('k' => 'v');
$o = (object)$a;
$back = (array)$o;
echo is_array($back) ? 'array' : 'not';
echo "\n";
?>
--EXPECT--
array
object
array
array
--CLEAN--
<?php
unset($scalar, $arr, $obj, $back_props, $a, $o, $back);
