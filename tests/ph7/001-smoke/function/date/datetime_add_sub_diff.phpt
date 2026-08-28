--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DateTime add/sub/diff/setISODate + procedural aliases
--FILE--
<?php
// diff: timelib's month-borrow breakdown (Jan 31 -> Mar 02 is 0 months 30 days)
$a = new DateTime('2024-01-31 10:00:00', new DateTimeZone('UTC'));
$b = new DateTime('2024-03-02 09:30:15', new DateTimeZone('UTC'));
$d = $a->diff($b);
echo $d->y, ' ', $d->m, ' ', $d->d, ' ', $d->h, ' ', $d->i, ' ', $d->s, ' ', $d->days, ' ', $d->invert, "\n";
$d2 = $b->diff($a);
echo $d2->invert, ' ', $d2->format('%R%a'), "\n";
echo $b->diff($a, true)->invert, "\n";

// add/sub with php's month-overflow spill
$c = new DateTime('2024-01-31', new DateTimeZone('UTC'));
$c->add(new DateInterval('P1M'));
echo $c->format('Y-m-d'), "\n";
$c->sub(new DateInterval('P1M'));
echo $c->format('Y-m-d'), "\n";
echo (new DateTimeImmutable('2024-03-31', new DateTimeZone('UTC')))->sub(new DateInterval('P1M'))->format('Y-m-d'), "\n";
$f = new DateTime('2024-01-15 23:00:00', new DateTimeZone('UTC'));
$f->add(new DateInterval('PT2H30M'));
echo $f->format('Y-m-d H:i'), "\n";

// diff across fixed offsets compares instants, breaks down in $this's zone
$m1 = new DateTime('2024-01-15 10:00:00', new DateTimeZone('+00:00'));
$m2 = new DateTime('2024-01-15 12:00:00', new DateTimeZone('+02:00'));
echo $m1->diff($m2)->format('%R %y %m %d %h %i %s %a'), "\n";
$m3 = new DateTime('2024-02-20 12:00:00', new DateTimeZone('+02:00'));
echo $m1->diff($m3)->format('%R %y %m %d %h %i %s %a'), "\n";

// setISODate (time of day preserved)
$s = new DateTime('2024-06-15 08:09:10', new DateTimeZone('UTC'));
$s->setISODate(2024, 1);
echo $s->format('Y-m-d H:i:s'), "\n";
$s->setISODate(2026, 53, 7);
echo $s->format('Y-m-d'), "\n";
echo (new DateTimeImmutable('2024-06-15', new DateTimeZone('UTC')))->setISODate(2025, 1, 3)->format('Y-m-d'), "\n";

// procedural aliases
$d1 = date_create('2024-01-01', new DateTimeZone('UTC'));
$d2b = date_create('2024-02-15', new DateTimeZone('UTC'));
echo date_diff($d1, $d2b)->format('%m months %d days (%a)'), "\n";
date_add($d1, date_interval_create_from_date_string('10 days'));
echo date_format($d1, 'Y-m-d'), "\n";
date_sub($d1, new DateInterval('P1D'));
echo $d1->format('Y-m-d'), "\n";
echo date_interval_format(date_diff($d1, $d2b), '%a days'), "\n";
echo timezone_name_get(timezone_open('+05:30')), ' ', timezone_offset_get(timezone_open('UTC'), $d1), "\n";
echo date_offset_get($d2b), ' ', date_timestamp_get($d2b), "\n";
date_isodate_set($d1, 2024, 2, 2);
echo $d1->format('Y-m-d'), "\n";

// createFrom* conversions keep instant + zone
$src = new DateTime('2024-06-01 12:00:00', new DateTimeZone('+02:00'));
$imm = DateTimeImmutable::createFromMutable($src);
echo get_class($imm), ' ', $imm->format('c e'), "\n";
echo get_class(DateTime::createFromImmutable($imm)), ' ', DateTime::createFromImmutable($imm)->format('c'), "\n";
echo get_class(DateTimeImmutable::createFromInterface($src)), "\n";
?>
--EXPECT--
0 0 30 23 30 15 30 0
1 -30
0
2024-03-02
2024-02-02
2024-03-02
2024-01-16 01:30
+ 0 0 0 0 0 0 0
+ 0 1 5 0 0 0 36
2024-01-01 08:09:10
2027-01-03
2025-01-01
1 months 14 days (45)
2024-01-11
2024-01-10
36 days
+05:30 0
0 1707955200
2024-01-09
DateTimeImmutable 2024-06-01T12:00:00+02:00 +02:00
DateTime 2024-06-01T12:00:00+02:00
DateTimeImmutable
--CLEAN--
<?php
unset($a, $b, $d, $d2, $c, $f, $m1, $m2, $m3, $s, $d1, $d2b, $src, $imm);
