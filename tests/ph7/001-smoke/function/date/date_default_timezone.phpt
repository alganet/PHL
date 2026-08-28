--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: date_default_timezone_get/set, UTC date() default, mktime family
--FILE--
<?php
// php's date.timezone default is UTC — date() must not read the system zone
echo date_default_timezone_get(), "\n";
echo date('Y-m-d H:i:s T e O', 1700000000), "\n";
echo gmdate('Y-m-d H:i:s', 1700000000), "\n";

// set() stores the id verbatim like php; GMT flips the zone label
echo date_default_timezone_set('GMT') ? 'true' : 'false', "\n";
echo date('T e', 0), "\n";
echo date_default_timezone_set('utc') ? 'true' : 'false', "\n";
echo date_default_timezone_get(), "\n";

// mktime: verbatim years except php's legacy two-digit mapping,
// month/day/time overflow normalizes, gmmktime agrees under UTC
echo mktime(1, 2, 3, 4, 5, 2024), ' ', gmmktime(1, 2, 3, 4, 5, 2024), "\n";
echo mktime(0, 0, 0, 14, 1, 2024), "\n";
echo mktime(0, 0, 0, 1, 1, 70), ' ', mktime(0, 0, 0, 1, 1, 0), ' ', mktime(0, 0, 0, 1, 1, 100), "\n";
echo mktime(0, 0, 0, 1, 1, 101), "\n";
echo mktime(25, -30, 0, 1, 1, 2024), "\n";
echo mktime(0, 0, 0, 1, 0, 2024), "\n";
try {
    mktime();
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}

// date('U') must format the passed timestamp, not the current clock
echo date('U', 1700000000), "\n";
?>
--EXPECT--
UTC
2023-11-14 22:13:20 UTC UTC +0000
2023-11-14 22:13:20
true
GMT GMT
true
utc
1712278923 1712278923
1738368000
0 946684800 946684800
-58979923200
1704155400
1703980800
mktime() expects at least 1 argument, 0 given
1700000000
--CLEAN--
<?php
date_default_timezone_set('UTC');
