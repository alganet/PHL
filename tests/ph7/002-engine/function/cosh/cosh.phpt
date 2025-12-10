--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: cosh basic functionality
--FILE--
<?php
// Test basic cosh functionality
$result1 = cosh(0);
echo abs($result1 - 1.0) < 0.0001 ? "ZERO_OK\n" : "ZERO_FAIL: $result1\n";

// Test cosh(1) ≈ (e + 1/e)/2 ≈ 1.54308
$result2 = cosh(1);
$expected = (exp(1) + exp(-1)) / 2;
echo abs($result2 - $expected) < 0.0001 ? "ONE_OK\n" : "ONE_FAIL: $result2\n";

// Test cosh(-1) = cosh(1) (even function)
$result3 = cosh(-1);
$result3_pos = cosh(1);
echo abs($result3 - $result3_pos) < 0.0001 ? "NEG_ONE_OK\n" : "NEG_ONE_FAIL: $result3 vs $result3_pos\n";

// Test cosh(2)
$result4 = cosh(2);
$expected2 = (exp(2) + exp(-2)) / 2;
echo abs($result4 - $expected2) < 0.0001 ? "TWO_OK\n" : "TWO_FAIL: $result4\n";

// Test that cosh(x) >= 1 for all real x
$result5 = cosh(5);
echo $result5 >= 1 ? "POSITIVE_OK\n" : "POSITIVE_FAIL: $result5\n";

// Test return type
$result6 = cosh(1.5);
echo is_float($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ZERO_OK
ONE_OK
NEG_ONE_OK
TWO_OK
POSITIVE_OK
TYPE_OK
