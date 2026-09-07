--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array string keys and automatic indexing
--FILE--
<?php
// Test arrays with string keys that look like numbers
// This exercises hashmap.c key conversion logic

// String keys that look like integers
$arr1 = array("123" => "string_key", "456" => "another");
echo "String keys: ";
echo $arr1["123"] . " " . $arr1["456"] . "\n";

// Mixed string and integer keys
$arr2 = array();
$arr2["0"] = "zero_string";
$arr2[0] = "zero_int";
$arr2["1"] = "one_string";
$arr2[1] = "one_int";
print_r($arr2);

// Automatic indexing with string keys
$arr3 = array("first", "second");
$arr3["custom"] = "custom_key";
$arr3[] = "auto_index"; // This should get index 2
print_r($arr3);

// Test key collision and overwriting
$arr4 = array("123" => "original");
$arr4[123] = "overwritten"; // Integer key should overwrite string key
echo "Key collision result: " . $arr4[123] . "\n";

// Test with float keys (converted to int)
$arr5 = array(1.5 => "float_key", 2.9 => "another_float");
echo "Float keys: " . $arr5[1] . " " . $arr5[2] . "\n";

// Test empty string keys
$arr6 = array("" => "empty_key");
echo "Empty string key: " . $arr6[""] . "\n";

echo "Array string keys test completed\n";
?>
--EXPECTF--
%AString keys: string_key another%AArray%A(%A[0] => zero_int%A[1] => one_int%A)%A[0] => first%A[1] => second%A[custom] => custom_key%A[2] => auto_index%AKey collision result: overwritten%AImplicit conversion from float 1.5 to int loses precision%AImplicit conversion from float 2.9 to int loses precision%AFloat keys: float_key another_float%AEmpty string key: empty_key%AArray string keys test completed%A
--CLEAN--
<?php
unset($arr1, $arr2, $arr3, $arr4, $arr5, $arr6);
