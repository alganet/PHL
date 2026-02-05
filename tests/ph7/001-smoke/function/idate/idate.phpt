--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: idate basic functionality
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test basic idate functionality
$result1 = idate("Y");
echo is_int($result1) && $result1 > 2000 ? "YEAR_OK\n" : "YEAR_FAIL: $result1\n";

// Test month
$result2 = idate("m");
// PH7's idate uses zero-indexed months.
echo is_int($result2) && $result2 >= 0 && $result2 <= 11 ? "MONTH_OK\n" : "MONTH_FAIL: $result2\n";

// Test day
$result3 = idate("d");
echo is_int($result3) && $result3 >= 1 && $result3 <= 31 ? "DAY_OK\n" : "DAY_FAIL: $result3\n";

// Test hour
$result4 = idate("H");
echo is_int($result4) && $result4 >= 0 && $result4 <= 23 ? "HOUR_OK\n" : "HOUR_FAIL: $result4\n";

// Test minutes
$result5 = idate("i");
echo is_int($result5) && $result5 >= 0 && $result5 <= 59 ? "MINUTE_OK\n" : "MINUTE_FAIL: $result5\n";

// Test seconds
$result6 = idate("s");
echo is_int($result6) && $result6 >= 0 && $result6 <= 59 ? "SECOND_OK\n" : "SECOND_FAIL: $result6\n";

// Test with timestamp
$timestamp = 1609470000; // 2021-01-01 00:00:00
$result7 = idate("Y", $timestamp);
echo $result7 === 2021 ? "TIMESTAMP_OK\n" : "TIMESTAMP_FAIL: $result7\n";
?>
--EXPECT--
YEAR_OK
MONTH_OK
DAY_OK
HOUR_OK
MINUTE_OK
SECOND_OK
TIMESTAMP_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $result4, $result5, $result6, $timestamp, $result7);
