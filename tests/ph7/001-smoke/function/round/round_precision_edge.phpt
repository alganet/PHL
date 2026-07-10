--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
round() with precision edge cases (very high and negative)
--FILE--
<?php
// High precision beyond a double's significance: value returned unchanged.
$val1 = round(2.12345678901234567890, 50);
echo "round_precision_50=" . $val1 . "\n";

// Negative precision rounds to the left of the decimal point:
// round(2.5, -1) == 0.0 (byte-exact with PHP; cross-engine coverage lives
// in round_modes.phpt and round_negative_precision.phpt).
$val2 = round(2.5, -1);
echo "round_precision_-1=" . $val2 . "\n";
?>
--EXPECT--
round_precision_50=2.1234567890123
round_precision_-1=0
--CLEAN--
<?php
unset($val1, $val2);
