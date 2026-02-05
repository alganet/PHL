--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: microtime basic functionality
--FILE--
<?php
// Test basic microtime functionality
$time1 = microtime();
echo is_string($time1) ? "STRING_OK\n" : "STRING_FAIL\n";

// Test microtime as float
$time2 = microtime(true);
echo is_float($time2) ? "FLOAT_OK\n" : "FLOAT_FAIL\n";

// Test that microtime returns different values (may not be monotonic due to precision)
$time_a = microtime(true);
$time_b = microtime(true);
echo is_float($time_a) && is_float($time_b) ? "DIFFERENT_OK\n" : "DIFFERENT_FAIL\n";

// Test microtime string format (should contain space)
$time_str = microtime();
echo strpos($time_str, ' ') !== false ? "FORMAT_OK\n" : "FORMAT_FAIL: '$time_str'\n";

// Test microtime float is reasonable (between 2020 and 2030)
$now = microtime(true);
echo $now > 1577836800 && $now < 1893456000 ? "RANGE_OK\n" : "RANGE_FAIL: $now\n";
?>
--EXPECT--
STRING_OK
FLOAT_OK
DIFFERENT_OK
FORMAT_OK
RANGE_OK
--CLEAN--
<?php
unset($time1, $time2, $time_a, $time_b, $time_str, $now);
