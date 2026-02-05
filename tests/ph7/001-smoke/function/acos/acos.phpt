--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: acos basic functionality
--FILE--
<?php
// Test basic acos functionality
$result1 = acos(1);
echo abs($result1 - 0.0) < 0.0001 ? "ONE_OK\n" : "ONE_FAIL: $result1\n";

// Test acos(0) = π/2
$result2 = acos(0);
$pi_half = 3.141592653589793 / 2;
echo abs($result2 - $pi_half) < 0.0001 ? "ZERO_OK\n" : "ZERO_FAIL: $result2\n";

// Test acos(-1) = π
$result3 = acos(-1);
$pi = 3.141592653589793;
echo abs($result3 - $pi) < 0.0001 ? "NEG_ONE_OK\n" : "NEG_ONE_FAIL: $result3\n";

// Test acos(0.5) ≈ π/3
$result4 = acos(0.5);
$pi_third = 3.141592653589793 / 3;
echo abs($result4 - $pi_third) < 0.0001 ? "HALF_OK\n" : "HALF_FAIL: $result4\n";

// Test acos(-0.5) ≈ 2π/3
$result5 = acos(-0.5);
$two_pi_third = 2 * 3.141592653589793 / 3;
echo abs($result5 - $two_pi_third) < 0.0001 ? "NEG_HALF_OK\n" : "NEG_HALF_FAIL: $result5\n";

// Test return type
$result6 = acos(0.3);
echo is_float($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ONE_OK
ZERO_OK
NEG_ONE_OK
HALF_OK
NEG_HALF_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $pi_half, $result3, $pi, $result4, $pi_third, $result5, $two_pi_third, $result6);
