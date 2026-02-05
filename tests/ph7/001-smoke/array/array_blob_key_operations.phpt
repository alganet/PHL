--CREDITS--
Test for array operations with blob (string) keys
--TEST--
Array blob key operations and hashmap lookup
--FILE--
<?php
// Test array with string keys
$arr = array("key1" => 100, "key2" => 200, "key3" => 300);
echo "arr['key1']: {$arr['key1']}\n";
echo "arr['key2']: {$arr['key2']}\n";

// Test isset and array_key_exists with string keys
echo "isset(arr['key1']): " . (isset($arr['key1']) ? 'true' : 'false') . "\n";
echo "array_key_exists('key2', arr): " . (array_key_exists('key2', $arr) ? 'true' : 'false') . "\n";

// Test modification
$arr["key1"] = 150;
echo "arr['key1'] after modification: {$arr['key1']}\n";

// Test unset
unset($arr["key2"]);
echo "arr['key2'] after unset: " . (isset($arr['key2']) ? 'set' : 'not set') . "\n";

// Test with longer keys
$long_key = "very_long_key_with_many_characters_to_test_hashing";
$arr[$long_key] = 500;
echo "arr[long_key]: {$arr[$long_key]}\n";
?>
--EXPECT--
arr['key1']: 100
arr['key2']: 200
isset(arr['key1']): true
array_key_exists('key2', arr): true
arr['key1'] after modification: 150
arr['key2'] after unset: not set
arr[long_key]: 500
--CLEAN--
<?php
unset($arr, $long_key);
