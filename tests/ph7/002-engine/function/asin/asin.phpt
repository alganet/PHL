--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: asin basic functionality
--FILE--
<?php
// Test basic asin functionality
$result1 = asin(0);
echo abs($result1 - 0.0) < 0.0001 ? "ZERO_OK\n" : "ZERO_FAIL: $result1\n";

// Test asin(1) = π/2
$result2 = asin(1);
$pi_half = 3.141592653589793 / 2;
echo abs($result2 - $pi_half) < 0.0001 ? "ONE_OK\n" : "ONE_FAIL: $result2\n";

// Test asin(-1) = -π/2
$result3 = asin(-1);
$neg_pi_half = -3.141592653589793 / 2;
echo abs($result3 - $neg_pi_half) < 0.0001 ? "NEG_ONE_OK\n" : "NEG_ONE_FAIL: $result3\n";

// Test asin(0.5) ≈ π/6
$result4 = asin(0.5);
$pi_sixth = 3.141592653589793 / 6;
echo abs($result4 - $pi_sixth) < 0.0001 ? "HALF_OK\n" : "HALF_FAIL: $result4\n";

// Test asin(-0.5) ≈ -π/6
$result5 = asin(-0.5);
$neg_pi_sixth = -3.141592653589793 / 6;
echo abs($result5 - $neg_pi_sixth) < 0.0001 ? "NEG_HALF_OK\n" : "NEG_HALF_FAIL: $result5\n";

// Test return type
$result6 = asin(0.3);
echo is_float($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ZERO_OK
ONE_OK
NEG_ONE_OK
HALF_OK
NEG_HALF_OK
TYPE_OK
