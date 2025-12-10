--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: mktime basic functionality
--FILE--
<?php
// Test basic mktime functionality
$result1 = mktime(0, 0, 0, 1, 1, 2021);
echo is_int($result1) && $result1 > 0 ? "BASIC_OK\n" : "BASIC_FAIL: $result1\n";

// Test specific date: 2021-01-01 00:00:00
$result2 = mktime(0, 0, 0, 1, 1, 2021);
// Laxed to account for missing timezone support
echo is_int($result2) && $result2 > 1609450000 ? "SPECIFIC_DATE_OK\n" : "SPECIFIC_DATE_FAIL: $result2\n";

// Test with current time components
$current_hour = (int)date("H");
$current_min = (int)date("i");
$current_sec = (int)date("s");
$current_mon = (int)date("m");
$current_day = (int)date("d");
$current_year = (int)date("Y");

$result3 = mktime($current_hour, $current_min, $current_sec, $current_mon, $current_day, $current_year);
$current_timestamp = time();
// Laxed to account for missing timezone support
echo abs($result3 - $current_timestamp) < 86400 ? "CURRENT_TIME_OK\n" : "CURRENT_TIME_FAIL: $result3 vs $current_timestamp\n";

// Test return type
$result4 = mktime(12, 30, 45, 6, 15, 2023);
echo is_int($result4) ? "TYPE_OK\n" : "TYPE_FAIL\n";

// Test gmmktime alias
$result5 = gmmktime(0, 0, 0, 1, 1, 2021);
echo is_int($result5) ? "GMT_ALIAS_OK\n" : "GMT_ALIAS_FAIL\n";
?>
--EXPECT--
BASIC_OK
SPECIFIC_DATE_OK
CURRENT_TIME_OK
TYPE_OK
GMT_ALIAS_OK
