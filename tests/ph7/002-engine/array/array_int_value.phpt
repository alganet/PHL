--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array to integer conversion
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test empty array
$empty = array();
$result = (int)$empty;
echo "Empty array: " . $result . "\n";

// Test array with elements
$array = array(1, 2, 3);
$result2 = (int)$array;
echo "Array with 3 elements: " . $result2 . "\n";

// Test associative array
$assoc = array("a" => 1, "b" => 2);
$result3 = (int)$assoc;
echo "Associative array: " . $result3 . "\n";
?>
--EXPECT--
Empty array: 0
Array with 3 elements: 3
Associative array: 2
--CLEAN--
<?php unset($empty, $result, $array, $result2, $assoc, $result3); ?>