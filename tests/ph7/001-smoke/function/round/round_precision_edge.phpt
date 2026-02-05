--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: round() with precision edge cases (> 30 and < 0)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with precision > 30 (should be capped at 30)
$val1 = round(2.12345678901234567890, 50);
echo "round_precision_50=" . $val1 . "\n";

// Test with precision < 0 (should be capped at 0)
$val2 = round(2.5, -1);
echo "round_precision_-1=" . $val2 . "\n";
?>
--EXPECT--
round_precision_50=2.12345678901234
round_precision_-1=3
--CLEAN--
<?php
unset($val1, $val2);
