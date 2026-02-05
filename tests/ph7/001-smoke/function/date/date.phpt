--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: date basic functionality
--FILE--
<?php

// Test basic date functionality
$result1 = date("Y");
echo strlen($result1) === 4 ? "YEAR_OK\n" : "YEAR_FAIL: '$result1'\n";

// Test date format
$result2 = date("Y-m-d");
echo strlen($result2) === 10 ? "DATE_FORMAT_OK\n" : "DATE_FORMAT_FAIL: '$result2'\n";

// Test with timestamp
$timestamp = 1609470000; // 2021-01-01 00:00:00
$result3 = date("Y-m-d", $timestamp);
echo $result3 === "2021-01-01" ? "TIMESTAMP_OK\n" : "TIMESTAMP_FAIL: '$result3'\n";

// Test time format
$result4 = date("H:i:s");
echo strlen($result4) === 8 ? "TIME_FORMAT_OK\n" : "TIME_FORMAT_FAIL: '$result4'\n";

// Test day of week
$result5 = date("l");
echo is_string($result5) && strlen($result5) > 0 ? "DAY_OF_WEEK_OK\n" : "DAY_OF_WEEK_FAIL: '$result5'\n";

// Test month name
$result6 = date("F");
echo is_string($result6) && strlen($result6) > 0 ? "MONTH_NAME_OK\n" : "MONTH_NAME_FAIL: '$result6'\n";

// Test return type
$result7 = date("Y-m-d");
echo is_string($result7) ? "TYPE_OK\n" : "TYPE_FAIL\n";
?>
--EXPECT--
YEAR_OK
DATE_FORMAT_OK
TIMESTAMP_OK
TIME_FORMAT_OK
DAY_OF_WEEK_OK
MONTH_NAME_OK
TYPE_OK
--CLEAN--
<?php
unset($result1, $result2, $timestamp, $result3, $result4, $result5, $result6, $result7);
