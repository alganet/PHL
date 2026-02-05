--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array_flip with duplicate values (overwrites)
--FILE--
<?php
// Test with duplicate values - later keys should overwrite earlier ones
$array = array('a' => 'value1', 'b' => 'value2', 'c' => 'value1');
$flipped = array_flip($array);

// Should have 2 elements since 'value1' appears twice
echo "Count: " . (count($flipped) == 2 ? "PASS" : "FAIL") . "\n";

// 'value1' should map to 'c' (last key with that value)
echo "value1 maps to: " . ($flipped['value1'] === 'c' ? "PASS" : "FAIL") . "\n";

// 'value2' should map to 'b'
echo "value2 maps to: " . ($flipped['value2'] === 'b' ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Count: PASS
value1 maps to: PASS
value2 maps to: PASS
--CLEAN--
<?php
unset($array, $flipped);
