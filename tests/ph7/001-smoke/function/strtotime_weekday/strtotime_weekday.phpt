--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtotime/DateTime parse weekday relatives (bare, next, last/previous) with optional time
--FILE--
<?php
function weekdayCases(): array {
    date_default_timezone_set("UTC");
    $b = 1600000000; // Sunday 2020-09-13
    $inputs = [
        "monday", "sunday", "thursday", "saturday", "friday", "tuesday", "wednesday",
        "next thursday", "last monday", "previous friday", "next sunday", "last sunday",
        "next monday", "this friday", "next saturday", "last saturday",
        "next thursday 15:00", "monday 08:30:15", "last friday 23:59:59",
        "sun", "next wed", "last tue",
    ];
    $out = [];
    foreach ($inputs as $in) { $out[] = $in . " => " . gmdate("D Y-m-d H:i:s", strtotime($in, $b)); }
    return $out;
}
echo implode("\n", weekdayCases()), "\n";
--EXPECT--
monday => Mon 2020-09-14 00:00:00
sunday => Sun 2020-09-13 00:00:00
thursday => Thu 2020-09-17 00:00:00
saturday => Sat 2020-09-19 00:00:00
friday => Fri 2020-09-18 00:00:00
tuesday => Tue 2020-09-15 00:00:00
wednesday => Wed 2020-09-16 00:00:00
next thursday => Thu 2020-09-17 00:00:00
last monday => Mon 2020-09-07 00:00:00
previous friday => Fri 2020-09-11 00:00:00
next sunday => Sun 2020-09-20 00:00:00
last sunday => Sun 2020-09-06 00:00:00
next monday => Mon 2020-09-14 00:00:00
this friday => Fri 2020-09-18 00:00:00
next saturday => Sat 2020-09-19 00:00:00
last saturday => Sat 2020-09-12 00:00:00
next thursday 15:00 => Thu 2020-09-17 15:00:00
monday 08:30:15 => Mon 2020-09-14 08:30:15
last friday 23:59:59 => Fri 2020-09-11 23:59:59
sun => Sun 2020-09-13 00:00:00
next wed => Wed 2020-09-16 00:00:00
last tue => Tue 2020-09-08 00:00:00
--CLEAN--
<?php
