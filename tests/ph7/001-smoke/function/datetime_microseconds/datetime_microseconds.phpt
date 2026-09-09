--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DateTime carries microseconds through parse, @epoch fraction, format u/v, setTime and createFromFormat
--FILE--
<?php
function microsecondCases(): array {
    date_default_timezone_set("UTC");
    $out = [];
    $out[] = (new DateTime("2020-01-01 12:00:00.123456"))->format("Y-m-d H:i:s.u");
    $out[] = (new DateTime("2020-01-01 12:00:00.5"))->format("u v");
    $out[] = (new DateTime("@1600000000.5"))->format("u");
    $out[] = (new DateTime("2020-06-15 08:30:00.999999"))->format("u");
    $out[] = (new DateTimeImmutable("2020-01-01 00:00:00.001"))->format("u v");
    $out[] = (new DateTime("2020-01-01 12:00:00"))->format("u");
    $out[] = DateTime::createFromFormat("Y-m-d H:i:s.u", "2020-01-01 12:00:00.654321")->format("u v");
    $d = new DateTime("2020-01-01 00:00:00.5");
    $d->setTime(10, 20, 30, 123456);
    $out[] = $d->format("H:i:s.u");
    $d->setTimestamp(1600000000);
    $out[] = $d->format("s.u");
    $d2 = new DateTime("2020-01-01 12:00:00.777");
    $d2->modify("+1 day");
    $out[] = $d2->format("s.u");
    $out[] = (new DateTimeImmutable("2020-06-15 08:30:00.999999"))->setTime(1, 2, 3, 4)->format("H:i:s.u");
    return $out;
}
echo implode("\n", microsecondCases()), "\n";
--EXPECT--
2020-01-01 12:00:00.123456
500000 500
500000
999999
001000 001
000000
654321 654
10:20:30.123456
40.000000
00.777000
01:02:03.000004
--CLEAN--
<?php
