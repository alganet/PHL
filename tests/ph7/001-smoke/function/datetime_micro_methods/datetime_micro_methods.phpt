--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DateTime getMicrosecond/setMicrosecond (php 8.4) and createFrom* microsecond copy
--FILE--
<?php
function microMethodCases(): array {
    date_default_timezone_set("UTC");
    $out = [];
    $d = new DateTime("2020-01-01 00:00:00.123456");
    $out[] = (string)$d->getMicrosecond();
    $out[] = $d->setMicrosecond(999)->format("u");
    $out[] = (string)$d->getMicrosecond();
    $i = new DateTimeImmutable("2020-01-01 00:00:00.5");
    $j = $i->setMicrosecond(42);
    $out[] = $i->getMicrosecond() . " " . $j->getMicrosecond();
    $out[] = (string)DateTime::createFromInterface(new DateTimeImmutable("2020-01-01 00:00:00.777"))->getMicrosecond();
    $out[] = (string)DateTimeImmutable::createFromMutable(new DateTime("2020-06-15 08:00:00.314159"))->getMicrosecond();
    return $out;
}
echo implode("\n", microMethodCases()), "\n";
--EXPECT--
123456
000999
999
500000 42
777000
314159
--CLEAN--
<?php
