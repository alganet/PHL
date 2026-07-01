--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: round() with precision edge cases (very high and negative)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// High precision beyond a double's significance: value returned unchanged.
// (PHL-only: the float->string echo prints one more significant digit than
// PHP here -- a pre-existing formatter divergence, not a round() difference,
// so this stays gated with --SKIPIF--.)
$val1 = round(2.12345678901234567890, 50);
echo "round_precision_50=" . $val1 . "\n";

// Negative precision rounds to the left of the decimal point:
// round(2.5, -1) == 0.0 (byte-exact with PHP; cross-engine coverage lives
// in round_modes.phpt and round_negative_precision.phpt).
$val2 = round(2.5, -1);
echo "round_precision_-1=" . $val2 . "\n";
?>
--EXPECT--
round_precision_50=2.12345678901234
round_precision_-1=0
--CLEAN--
<?php
unset($val1, $val2);
