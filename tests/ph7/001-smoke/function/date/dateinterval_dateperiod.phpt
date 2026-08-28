--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DateInterval + DatePeriod (band D slice 2)
--FILE--
<?php
$i = new DateInterval('P1Y2M3DT4H5M6S');
echo $i->y, ' ', $i->m, ' ', $i->d, ' ', $i->h, ' ', $i->i, ' ', $i->s, ' ',
     $i->f, ' ', $i->invert, ' ', var_export($i->days, true), "\n";
echo (new DateInterval('P2W'))->d, ' ', (new DateInterval('P1W2D'))->d, "\n";
foreach (['bogus', 'P1X', 'P', 'PT', 'P5'] as $bad) {
    try {
        new DateInterval($bad);
        echo "OK($bad)\n";
    } catch (DateMalformedIntervalStringException $e) {
        echo $e->getMessage(), "\n";
    }
}
$cd = DateInterval::createFromDateString('1 year + 3 hours');
echo $cd->y, ' ', $cd->h, "\n";
echo date_interval_create_from_date_string('2 weeks')->d, "\n";
echo $i->format('%y-%m-%d %h:%i:%s %R%% %a'), "\n";
echo (new DateInterval('P1YT61S'))->format('%Y|%M|%D|%H|%I|%S|%y|%s|%R|%r|%a|%F|%f|%%'), "\n";

// DatePeriod: recurrence form, end form, flags, ISO string
$p = new DatePeriod(new DateTime('2024-01-01', new DateTimeZone('UTC')), new DateInterval('P1W'), 3);
$o = [];
foreach ($p as $k => $dt) { $o[] = $k . ':' . $dt->format('Y-m-d'); }
echo implode(' ', $o), "\n";
echo get_class($p->getStartDate()), ' ', var_export($p->getEndDate(), true), ' ', $p->getRecurrences(), "\n";
echo $p->recurrences, ' ', $p->include_start_date ? 'incl' : 'excl', "\n";
$q = new DatePeriod(new DateTime('2024-01-01', new DateTimeZone('UTC')), new DateInterval('P10D'), new DateTime('2024-02-01', new DateTimeZone('UTC')));
$o = [];
foreach ($q as $dt) { $o[] = $dt->format('m-d'); }
echo implode(' ', $o), "\n";
$r = new DatePeriod(new DateTime('2024-01-01', new DateTimeZone('UTC')), new DateInterval('P1D'), 2, DatePeriod::EXCLUDE_START_DATE);
$o = [];
foreach ($r as $k => $dt) { $o[] = $k . ':' . $dt->format('m-d'); }
echo implode(' ', $o), "\n";
$r2 = new DatePeriod(new DateTime('2024-01-01', new DateTimeZone('UTC')), new DateInterval('P1D'), 2, DatePeriod::INCLUDE_END_DATE);
$o = [];
foreach ($r2 as $dt) { $o[] = $dt->format('m-d'); }
echo implode(' ', $o), "\n";
$s = DatePeriod::createFromISO8601String('R2/2024-07-01T00:00:00Z/P1D');
$o = [];
foreach ($s as $dt) { $o[] = $dt->format('m-d'); }
echo implode(' ', $o), "\n";
echo get_class($s->getStartDate()), "\n";
$im = new DatePeriod(new DateTimeImmutable('2024-01-31', new DateTimeZone('UTC')), new DateInterval('P1M'), 2);
$o = [];
foreach ($im as $dt) { $o[] = get_class($dt) . ':' . $dt->format('m-d'); }
echo implode(' ', $o), "\n";
?>
--EXPECT--
1 2 3 4 5 6 0 0 false
14 9
Unknown or bad format (bogus)
Unknown or bad format (P1X)
Unknown or bad format (P)
Unknown or bad format (PT)
Unknown or bad format (P5)
1 3
14
1-2-3 4:5:6 +% (unknown)
01|00|00|00|00|61|1|61|+||(unknown)|000000|0|%
0:2024-01-01 1:2024-01-08 2:2024-01-15 3:2024-01-22
DateTime NULL 3
4 incl
01-01 01-11 01-21 01-31
0:01-02 1:01-03
01-01 01-02 01-03 01-04
07-01 07-02 07-03
DateTimeImmutable
DateTimeImmutable:01-31 DateTimeImmutable:03-02 DateTimeImmutable:04-02
--CLEAN--
<?php
unset($i, $cd, $p, $q, $r, $r2, $s, $im, $k, $dt, $bad, $o);
