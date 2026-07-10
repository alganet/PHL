--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
tan with very large value (edge case)
--FILE--
<?php
// Test tan with very large value - covers potential edge case handling
$large_value = 1e20; // Very large number
$result = tan($large_value);
echo "tan(1e20) = " . $result . "\n";
if (is_numeric($result)) {
    echo "PASS: Returned numeric value\n";
} else {
    echo "FAIL: Did not return numeric value\n";
}
?>
--EXPECT--
tan(1e20) = -0.84460246301988
PASS: Returned numeric value
--CLEAN--
<?php
unset($large_value, $result);
