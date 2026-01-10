--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations with mixed key and value types
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
/* Test array with mixed types to exercise hashmap with different data */
$a = array();
$a["zero"] = "string";
$a["one"] = 42;
$a[2.5] = true;
$a["null_key"] = array("nested" => "array");
$a["false_key"] = null;

/* Perform operations */
echo "Count: " . count($a) . "\n";
echo "Key zero: " . $a["zero"] . "\n";
echo "Key one: " . $a["one"] . "\n";
echo "Key 2.5: " . ($a[2.5] ? 'true' : 'false') . "\n";
echo "Key null_key: " . (is_array($a["null_key"]) ? 'array' : 'not_array') . "\n";
echo "Key false_key: " . (is_null($a["false_key"]) ? 'null' : 'not_null') . "\n";

/* Test isset and empty */
echo "Isset key zero: " . (isset($a["zero"]) ? 'yes' : 'no') . "\n";
echo "Isset key 'missing': " . (isset($a["missing"]) ? 'yes' : 'no') . "\n";
echo "Empty key false_key: " . (empty($a["false_key"]) ? 'yes' : 'no') . "\n";

echo "Test completed\n";
?>
--EXPECT--
Count: 5
Key zero: string
Key one: 42
Key 2.5: true
Key null_key: array
Key false_key: null
Isset key zero: yes
Isset key 'missing': no
Empty key false_key: yes
Test completed
