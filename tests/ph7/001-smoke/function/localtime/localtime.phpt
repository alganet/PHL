--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: localtime basic functionality
--FILE--
<?php
// Test basic localtime functionality
$time_info = localtime();
echo is_array($time_info) ? "ARRAY_OK\n" : "ARRAY_FAIL\n";

// Test that array has correct number of elements
echo count($time_info) === 9 ? "COUNT_OK\n" : "COUNT_FAIL: " . count($time_info) . "\n";

// Test with timestamp
$timestamp = 1609470000; // 2021-01-01 00:00:00
$time_info2 = localtime($timestamp);
echo is_array($time_info2) && count($time_info2) === 9 ? "TIMESTAMP_OK\n" : "TIMESTAMP_FAIL\n";

// Test second associative mode
$time_info3 = localtime(time(), true);
echo is_array($time_info3) ? "ASSOC_OK\n" : "ASSOC_FAIL\n";

// Test that values are reasonable (seconds between 0-59)
$current = localtime();
$seconds = $time_info[0];
echo is_int($seconds) && $seconds >= 0 && $seconds <= 59 ? "SECONDS_OK\n" : "SECONDS_FAIL: $seconds\n";

// Test that values are reasonable (minutes between 0-59)
$minutes = $time_info[1];
echo is_int($minutes) && $minutes >= 0 && $minutes <= 59 ? "MINUTES_OK\n" : "MINUTES_FAIL: $minutes\n";
?>
--EXPECT--
ARRAY_OK
COUNT_OK
TIMESTAMP_OK
ASSOC_OK
SECONDS_OK
MINUTES_OK
--CLEAN--
<?php
unset($time_info, $timestamp, $time_info2, $time_info3, $current, $seconds, $minutes);
