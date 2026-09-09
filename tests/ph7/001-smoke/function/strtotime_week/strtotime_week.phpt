--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtotime/DateTime parse this|next|last week (Monday-of-week) navigation
--FILE--
<?php
function weekNavCases(): array {
    date_default_timezone_set("UTC");
    $b = 1600000000; // Sunday 2020-09-13 12:26:40
    $inputs = [
        "this week", "next week", "last week",
        "next week 09:00", "this week midnight", "last week 23:00",
        "this month", "next month", "last month",
    ];
    $out = [];
    foreach ($inputs as $in) { $out[] = $in . " => " . gmdate("D Y-m-d H:i:s", strtotime($in, $b)); }
    return $out;
}
echo implode("\n", weekNavCases()), "\n";
--EXPECT--
this week => Mon 2020-09-07 12:26:40
next week => Mon 2020-09-14 12:26:40
last week => Mon 2020-08-31 12:26:40
next week 09:00 => Mon 2020-09-14 09:00:00
this week midnight => Mon 2020-09-07 00:00:00
last week 23:00 => Mon 2020-08-31 23:00:00
this month => Sun 2020-09-13 12:26:40
next month => Tue 2020-10-13 12:26:40
last month => Thu 2020-08-13 12:26:40
--CLEAN--
<?php
