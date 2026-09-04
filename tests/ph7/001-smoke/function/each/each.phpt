--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
each() function basic usage
--SKIPIF--
<?php
// PHL extension: `each()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: each() is not a php symbol'; }
?>
--FILE--
<?php
// Test each() function with associative array
$arr = array('a' => 1, 'b' => 2, 'c' => 3);
while (list($key, $value) = each($arr)) {
    echo "Key: $key, Value: $value\n";
}

// Test each() function with numeric array
$nums = array(10, 20, 30);
while (list($key, $value) = each($nums)) {
    echo "Index: $key, Value: $value\n";
}

// Test each() on empty array (should return false)
$empty = array();
$result = each($empty);
if ($result === false) {
    echo "Empty array test: returned false as expected\n";
}

// Test each() after reaching end of array
$single = array('one' => 1);
each($single); // First call
$result = each($single); // Second call should return false
if ($result === false) {
    echo "End of array test: returned false as expected\n";
}
?>
--EXPECT--
Key: a, Value: 1
Key: b, Value: 2
Key: c, Value: 3
Index: 0, Value: 10
Index: 1, Value: 20
Index: 2, Value: 30
Empty array test: returned false as expected
End of array test: returned false as expected
--CLEAN--
<?php
unset($arr, $nums, $empty, $result, $single);
