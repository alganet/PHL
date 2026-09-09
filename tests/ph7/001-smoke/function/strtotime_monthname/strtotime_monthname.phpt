--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtotime/DateTime parse textual-month dates (MonthName Day Year and Day MonthName Year)
--FILE--
<?php
function monthNameCases(): array {
    date_default_timezone_set("UTC");
    $inputs = [
        "Jan 15 2020", "January 15 2020", "15 January 2020", "15 Jan 2020",
        "January 15, 2020", "15th January 2020", "January 15th 2020",
        "1 January 2020", "15 JANUARY 2020", "15 jan 2020",
        "Jan 15 2020 14:30:00", "15 January 2020 08:00",
        "January 2020", "Jan 2020", "15 January", "January 15",
        "Feb 29 2020", "Feb 30 2020", "Xyz 15 2020", "15 Foo 2020",
        "Sep 9 2001", "December 31 1999", "2020 Jan 15", "January 15 2020 UTC",
        "sept 5 2020", "15 May 2020", "31st December 2020", "November 30th, 2019",
    ];
    $out = [];
    foreach ($inputs as $in) { $out[] = $in . " => " . var_export(strtotime($in, 1600000000), true); }
    return $out;
}
echo implode("\n", monthNameCases()), "\n";
--EXPECT--
Jan 15 2020 => 1579046400
January 15 2020 => 1579046400
15 January 2020 => 1579046400
15 Jan 2020 => 1579046400
January 15, 2020 => 1579046400
15th January 2020 => 1579046400
January 15th 2020 => 1579046400
1 January 2020 => 1577836800
15 JANUARY 2020 => 1579046400
15 jan 2020 => 1579046400
Jan 15 2020 14:30:00 => 1579098600
15 January 2020 08:00 => 1579075200
January 2020 => 1577836800
Jan 2020 => 1577836800
15 January => 1579046400
January 15 => 1579046400
Feb 29 2020 => 1582934400
Feb 30 2020 => 1583020800
Xyz 15 2020 => false
15 Foo 2020 => false
Sep 9 2001 => 999993600
December 31 1999 => 946598400
2020 Jan 15 => false
January 15 2020 UTC => 1579046400
sept 5 2020 => 1599264000
15 May 2020 => 1589500800
31st December 2020 => 1609372800
November 30th, 2019 => 1575072000
--CLEAN--
<?php
