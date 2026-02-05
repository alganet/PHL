--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: log10 basic functionality
--FILE--
<?php
// Test basic log10 functionality
$result1 = log10(1);
echo abs($result1 - 0.0) < 0.0001 ? "ONE_OK\n" : "ONE_FAIL: $result1\n";

// Test log10(10) = 1
$result2 = log10(10);
echo abs($result2 - 1.0) < 0.0001 ? "TEN_OK\n" : "TEN_FAIL: $result2\n";

// Test log10(100) = 2
$result3 = log10(100);
echo abs($result3 - 2.0) < 0.0001 ? "HUNDRED_OK\n" : "HUNDRED_FAIL: $result3\n";

// Test log10(0.1) = -1
$result4 = log10(0.1);
echo abs($result4 + 1.0) < 0.0001 ? "TENTH_OK\n" : "TENTH_FAIL: $result4\n";

// Test relationship: log10(x) = ln(x) / ln(10)
$result5 = log10(50);
$result5_alt = log(50) / log(10);
echo abs($result5 - $result5_alt) < 0.0001 ? "RELATION_OK\n" : "RELATION_FAIL: $result5 vs $result5_alt\n";

// Test return type
$result6 = log10(2.5);
echo is_float($result6) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
ONE_OK
TEN_OK
HUNDRED_OK
TENTH_OK
RELATION_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result5_alt, $result6);
