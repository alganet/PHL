--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DateTime / DateTimeImmutable / DateTimeZone core (band D slice 1)
--FILE--
<?php
$d = new DateTime('2024-01-15 10:30:00', new DateTimeZone('UTC'));
echo $d->getTimestamp(), "\n";
echo $d->format('Y-m-d H:i:s'), "\n";
$d->modify('+1 day');
echo $d->format('Y-m-d'), "\n";
$d->modify('+2 months');
echo $d->format('Y-m-d'), "\n";

// Immutable: modify returns a new object, base unchanged; Jan 31 overflow
$i = new DateTimeImmutable('2024-01-31', new DateTimeZone('UTC'));
$j = $i->modify('+1 month');
echo $i->format('Y-m-d'), ' ', $j->format('Y-m-d'), "\n";

// @timestamp and fixed-offset zones
$e = new DateTime('@1700000000');
echo $e->getTimestamp(), ' ', $e->getTimezone()->getName(), "\n";
$f = new DateTime('2024-06-01 08:30:00', new DateTimeZone('+05:30'));
echo $f->getTimestamp(), ' ', $f->getOffset(), ' ', $f->format('P'), "\n";

// Offset parsed from the string wins over the zone argument
$g = new DateTime('2024-06-01T08:30:00+02:00', new DateTimeZone('UTC'));
echo $g->getTimestamp(), ' ', $g->getOffset(), "\n";

// Setters
$s = new DateTime('2024-01-15 10:30:00', new DateTimeZone('UTC'));
$s->setDate(2025, 3, 9);
$s->setTime(4, 5, 6);
echo $s->format('Y-m-d H:i:s'), "\n";
$s->setTimestamp(1600000000);
echo $s->format('Y-m-d H:i:s'), "\n";

// date_create returns false on garbage, object on success
$ok = date_create('2024-02-29 12:00:00', new DateTimeZone('UTC'));
echo $ok === false ? 'false' : $ok->format('Y-m-d H:i:s'), "\n";
echo date_create('total nonsense') === false ? 'false' : 'object', "\n";
$im = date_create_immutable('@123456');
echo $im instanceof DateTimeImmutable ? 'immutable' : 'plain', "\n";

// GMT zone identity
$z = new DateTimeZone('GMT');
echo $z->getName(), ' ', $z->getOffset(new DateTime('@0')), "\n";
echo (new DateTimeZone('utc'))->getName(), "\n";
?>
--EXPECT--
1705314600
2024-01-15 10:30:00
2024-01-16
2024-03-16
2024-01-31 2024-03-02
1700000000 +00:00
1717210800 19800 +05:30
1717223400 7200
2025-03-09 04:05:06
2020-09-13 12:26:40
2024-02-29 12:00:00
false
immutable
GMT 0
UTC
--CLEAN--
<?php
unset($d, $i, $j, $e, $f, $g, $s, $ok, $im, $z);
