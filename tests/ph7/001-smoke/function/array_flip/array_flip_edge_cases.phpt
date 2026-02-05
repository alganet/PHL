--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
if (function_exists('zend_version')) echo 'skip';
?>
--TEST--
array_flip with NULL values and non-flippable types
--FILE--
<?php
// Test with NULL values
$array1 = array('a' => NULL, 'b' => 'value');
$flipped1 = array_flip($array1);
echo "Flipped array with NULL: " . (count($flipped1) == 1 ? "PASS" : "FAIL") . "\n";

// Test with array values (converted to string)
$array2 = array('a' => array(1,2), 'b' => 'value');
$flipped2 = array_flip($array2);
echo "Flipped array with sub-array: " . (count($flipped2) == 2 ? "PASS" : "FAIL") . "\n";

// Test with object (converted to string)
class TestObj {}
$obj = new TestObj();
$array3 = array('a' => $obj, 'b' => 'value');
$flipped3 = array_flip($array3);
echo "Flipped array with object: " . (count($flipped3) == 2 ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Flipped array with NULL: PASS
Flipped array with sub-array: PASS
Flipped array with object: PASS
--CLEAN--
<?php
unset($array1, $flipped1, $array2, $flipped2, $obj, $array3, $flipped3);
