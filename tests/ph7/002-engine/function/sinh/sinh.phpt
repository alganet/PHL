--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: sinh basic functionality
--FILE--
<?php
// Test basic sinh functionality
$result1 = sinh(0);
echo abs($result1 - 0.0) < 0.0001 ? "ZERO_OK\n" : "ZERO_FAIL: $result1\n";

// Test sinh(1) ≈ (e - 1/e)/2 ≈ 1.17520
$result2 = sinh(1);
$expected = (exp(1) - exp(-1)) / 2;
echo abs($result2 - $expected) < 0.0001 ? "ONE_OK\n" : "ONE_FAIL: $result2\n";

// Test sinh(-1) = -sinh(1) (odd function)
$result3 = sinh(-1);
$result3_pos = sinh(1);
echo abs($result3 + $result3_pos) < 0.0001 ? "NEG_ONE_OK\n" : "NEG_ONE_FAIL: $result3 vs $result3_pos\n";

// Test sinh(2)
$result4 = sinh(2);
$expected2 = (exp(2) - exp(-2)) / 2;
echo abs($result4 - $expected2) < 0.0001 ? "TWO_OK\n" : "TWO_FAIL: $result4\n";

// Test that sinh is odd function: sinh(-x) = -sinh(x)
$result5 = sinh(-3);
$result5_pos = sinh(3);
echo abs($result5 + $result5_pos) < 0.0001 ? "ODD_OK\n" : "ODD_FAIL: $result5 vs $result5_pos\n";

// Test return type
$result6 = sinh(1.5);
echo is_float($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ZERO_OK
ONE_OK
NEG_ONE_OK
TWO_OK
ODD_OK
TYPE_OK
