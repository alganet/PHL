--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtotime/DateTime parse first|last day of month and standalone month navigation
--FILE--
<?php
function dayOfCases(): array {
    date_default_timezone_set("UTC");
    $b = 1600000000;
    $inputs = [
        "first day of this month", "last day of this month", "first day of next month",
        "last day of next month", "first day of last month", "last day of last month",
        "first day of January 2020", "last day of February 2020", "first day of Feb 2021",
        "last day of December 2020", "first day of March", "last day of April",
        "next month", "last month", "this month",
        "first day of this month midnight", "first day of next month 09:00",
        "last day of",
    ];
    $out = [];
    foreach ($inputs as $in) { $out[] = $in . " => " . var_export(strtotime($in, $b), true); }
    return $out;
}
echo implode("\n", dayOfCases()), "\n";
--EXPECT--
first day of this month => 1598963200
last day of this month => 1601468800
first day of next month => 1601555200
last day of next month => 1604147200
first day of last month => 1596284800
last day of last month => 1598876800
first day of January 2020 => 1577836800
last day of February 2020 => 1582934400
first day of Feb 2021 => 1612137600
last day of December 2020 => 1609372800
first day of March => 1583020800
last day of April => 1588204800
next month => 1602592000
last month => 1597321600
this month => 1600000000
first day of this month midnight => 1598918400
first day of next month 09:00 => 1601542800
last day of => 1601468800
--CLEAN--
<?php
