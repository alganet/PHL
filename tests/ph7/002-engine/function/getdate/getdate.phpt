--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: getdate basic functionality
--FILE--
<?php
// Test basic getdate functionality
$date_info = getdate();
echo is_array($date_info) ? "ARRAY_OK\n" : "ARRAY_FAIL\n";

// Test that required keys exist
$required_keys = array('seconds', 'minutes', 'hours', 'mday', 'wday', 'mon', 'year', 'yday', 'weekday', 'month');
$has_keys = true;
foreach ($required_keys as $key) {
    if (!isset($date_info[$key])) {
        $has_keys = false;
        break;
    }
}
echo $has_keys ? "KEYS_OK\n" : "KEYS_FAIL\n";

// Test with timestamp
$timestamp = 1609459200; // 2021-01-01 00:00:00
$date_info2 = getdate($timestamp);
echo is_array($date_info2) ? "TIMESTAMP_OK\n" : "TIMESTAMP_FAIL\n";

// Test current time vs timestamp
$current = getdate();
$specific = getdate(time());
echo $current['year'] === $specific['year'] ? "CURRENT_OK\n" : "CURRENT_FAIL\n";
?>
--EXPECT--
ARRAY_OK
KEYS_OK
TIMESTAMP_OK
CURRENT_OK
