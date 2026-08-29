--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strftime basic functionality

--FILE--
<?php
set_error_handler(function(){ return true; }); // suppress the 8.1 strftime deprecation; the notice has its own test
// Test basic strftime functionality
$result1 = strftime("%Y");
echo is_string($result1) && strlen($result1) === 4 ? "YEAR_OK\n" : "YEAR_FAIL: '$result1'\n";

// Test month formatting
$result2 = strftime("%m");
echo is_string($result2) && strlen($result2) === 2 ? "MONTH_OK\n" : "MONTH_FAIL: '$result2'\n";

// Test day formatting
$result3 = strftime("%d");
echo is_string($result3) && strlen($result3) === 2 ? "DAY_OK\n" : "DAY_FAIL: '$result3'\n";

// Test with timestamp
$timestamp = 1609470000; // 2021-01-01 00:00:00
$result4 = strftime("%Y-%m-%d", $timestamp);
echo $result4 === "2021-01-01" ? "TIMESTAMP_OK\n" : "TIMESTAMP_FAIL: '$result4'\n";

// Test hour formatting
$result5 = strftime("%H");
echo is_string($result5) && strlen($result5) === 2 ? "HOUR_OK\n" : "HOUR_FAIL: '$result5'\n";

// Test minute formatting
$result6 = strftime("%M");
echo is_string($result6) && strlen($result6) === 2 ? "MINUTE_OK\n" : "MINUTE_FAIL: '$result6'\n";
?>
--EXPECT--
YEAR_OK
MONTH_OK
DAY_OK
TIMESTAMP_OK
HOUR_OK
MINUTE_OK
--CLEAN--
<?php
unset($result1, $result2, $result3, $timestamp, $result4, $result5, $result6);
