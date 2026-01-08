--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: tan with very large value (edge case)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test tan with very large value - covers potential edge case handling
$large_value = 1e20; // Very large number
$result = tan($large_value);
// PHL may return 0 for large values that cause floating point issues
echo "tan(1e20) = " . $result . "\n";
if (is_numeric($result)) {
    echo "PASS: Returned numeric value\n";
} else {
    echo "FAIL: Did not return numeric value\n";
}
?>
--EXPECT--
tan(1e20) = -0.844602463019884
PASS: Returned numeric value