--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: idate additional format tokens
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test L - leap year (1 if leap year, 0 otherwise) for non-leap year
$leap_ts = 1672531200; // 2023-01-01 (not leap)
$non_leap = idate("L", $leap_ts);
echo "Non-leap year L: $non_leap\n"; // Should be 0

// Test t - days in current month
$days = idate("t");
echo "Days in month: $days\n"; // Should be 28-31

// Test U - seconds since Unix epoch
$u = idate("U");
echo "Is positive int: " . (is_int($u) && $u > 0 ? "YES" : "NO") . "\n";

// Test w - day of week (0=Sunday)
$w = idate("w");
echo "Day of week (0-6): $w\n";

// Test W - ISO-8601 week number
$W = idate("W");
echo "Week number: $W\n";

// Test z - day of year (0-365)
$z = idate("z");
echo "Day of year (0-365): $z\n";

// Test Z - timezone offset in seconds
$Z = idate("Z");
echo "TZ offset (should be 0 for UTC): $Z\n";

// Test I (uppercase i) - DST indicator
$I = idate("I");
echo "DST indicator (0 or 1): $I\n";

// Test with timestamp - Y
$ts = 1609470000; // 2021-01-01 00:00:00
$y = idate("Y", $ts);
echo "Year from timestamp: $y\n"; // Should be 2021

// Test with timestamp - m
$m = idate("m", $ts);
echo "Month from timestamp (0-11): $m\n"; // PH7 uses 0-indexed months -> 0

// Test with timestamp - d
$d = idate("d", $ts);
echo "Day from timestamp: $d\n"; // Should be 1
?>
--EXPECTF--
Non-leap year L: 0
Days in month: %d
Is positive int: YES
Day of week (0-6): %d
Week number: %d
Day of year (0-365): %d
TZ offset (should be 0 for UTC): %d
DST indicator (0 or 1): %d
Year from timestamp: 2021
Month from timestamp (0-11): 0
Day from timestamp: 1
--CLEAN--
<?php
unset($leap_ts, $non_leap, $days, $u, $w, $W, $z, $Z, $I, $ts, $y, $m, $d);
