--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: atan basic functionality
--FILE--
<?php
// Test basic atan functionality
$result1 = atan(0);
echo abs($result1 - 0.0) < 0.0001 ? "ZERO_OK\n" : "ZERO_FAIL: $result1\n";

// Test atan(1) ≈ π/4
$result2 = atan(1);
$expected = 3.141592653589793 / 4;
echo abs($result2 - $expected) < 0.0001 ? "ONE_OK\n" : "ONE_FAIL: $result2\n";

// Test atan(-1) ≈ -π/4
$result3 = atan(-1);
$expected_neg = -3.141592653589793 / 4;
echo abs($result3 - $expected_neg) < 0.0001 ? "NEG_ONE_OK\n" : "NEG_ONE_FAIL: $result3\n";

// Test atan with large value approaches π/2
$result4 = atan(1000000);
$pi_half = 3.141592653589793 / 2;
echo abs($result4 - $pi_half) < 0.01 ? "LARGE_OK\n" : "LARGE_FAIL: $result4\n";

// Test atan with negative large value approaches -π/2
$result5 = atan(-1000000);
$neg_pi_half = -3.141592653589793 / 2;
echo abs($result5 - $neg_pi_half) < 0.01 ? "NEG_LARGE_OK\n" : "NEG_LARGE_FAIL: $result5\n";

// Test return type
$result6 = atan(0.5);
echo is_float($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ZERO_OK
ONE_OK
NEG_ONE_OK
LARGE_OK
NEG_LARGE_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $expected, $result3, $expected_neg, $result4, $pi_half, $result5, $neg_pi_half, $result6);
