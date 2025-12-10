--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: tanh basic functionality
--FILE--
<?php
// Test basic tanh functionality
$result1 = tanh(0);
echo abs($result1 - 0.0) < 0.0001 ? "ZERO_OK\n" : "ZERO_FAIL: $result1\n";

// Test tanh(1) ≈ sinh(1)/cosh(1)
$result2 = tanh(1);
$expected = sinh(1) / cosh(1);
echo abs($result2 - $expected) < 0.0001 ? "ONE_OK\n" : "ONE_FAIL: $result2\n";

// Test tanh(-1) = -tanh(1) (odd function)
$result3 = tanh(-1);
$result3_pos = tanh(1);
echo abs($result3 + $result3_pos) < 0.0001 ? "NEG_ONE_OK\n" : "NEG_ONE_FAIL: $result3 vs $result3_pos\n";

// Test that tanh(x) is between -1 and 1 for all real x
$result4 = tanh(10);
echo $result4 > -1 && $result4 < 1 ? "RANGE_POS_OK\n" : "RANGE_POS_FAIL: $result4\n";

$result5 = tanh(-10);
echo $result5 > -1 && $result5 < 1 ? "RANGE_NEG_OK\n" : "RANGE_NEG_FAIL: $result5\n";

// Test tanh approaches 1 as x approaches infinity
$result6 = tanh(100);
echo abs($result6 - 1.0) < 0.0001 ? "LIMIT_POS_OK\n" : "LIMIT_POS_FAIL: $result6\n";

// Test tanh approaches -1 as x approaches negative infinity
$result7 = tanh(-100);
echo abs($result7 + 1.0) < 0.0001 ? "LIMIT_NEG_OK\n" : "LIMIT_NEG_FAIL: $result7\n";

// Test return type
$result8 = tanh(0.5);
echo is_float($result8) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ZERO_OK
ONE_OK
NEG_ONE_OK
RANGE_POS_OK
RANGE_NEG_OK
LIMIT_POS_OK
LIMIT_NEG_OK
TYPE_OK
