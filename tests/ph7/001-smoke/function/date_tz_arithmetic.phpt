--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Date arithmetic in a database zone is WALL-CLOCK arithmetic
--FILE--
<?php
/* php's date arithmetic is WALL-CLOCK arithmetic. `+1 day` over a spring-forward
 * morning is 23 hours of real time and the clock still reads the same; six
 * months from a January date crosses into daylight time and the hour does not
 * move. Every door that walks the civil fields owes the zone a second look --
 * modify(), add()/sub(), setDate(), setTime(), setISODate() and the period walk
 * -- while setTimestamp(), which names an INSTANT rather than a reading, owes
 * only the offset that instant is on. */
date_default_timezone_set('UTC');
$f = 'Y-m-d H:i:s P T I';

foreach (['America/New_York', 'Europe/Paris', 'Australia/Sydney'] as $zone) {
    $z = new DateTimeZone($zone);
    foreach (['2010-01-15 10:00:00', '2010-03-13 12:00:00', '2010-11-06 12:00:00'] as $s) {
        $d = new DateTime($s, $z);
        printf("%-18s %s base    %s\n", $zone, $s, $d->format($f));
        printf("%-18s %s +6month %s\n", $zone, $s, (clone $d)->modify('+6 months')->format($f));
        printf("%-18s %s +1day   %s\n", $zone, $s, (clone $d)->modify('+1 day')->format($f));
        printf("%-18s %s +24hour %s\n", $zone, $s, (clone $d)->modify('+24 hours')->format($f));
        printf("%-18s %s addP1M  %s\n", $zone, $s, (clone $d)->add(new DateInterval('P1M'))->format($f));
        printf("%-18s %s subP1M  %s\n", $zone, $s, (clone $d)->sub(new DateInterval('P1M'))->format($f));
        printf("%-18s %s setDate %s\n", $zone, $s, (clone $d)->setDate(2010, 8, 3)->format($f));
        printf("%-18s %s setTime %s\n", $zone, $s, (clone $d)->setTime(2, 30, 15)->format($f));
        printf("%-18s %s setISO  %s\n", $zone, $s, (clone $d)->setISODate(2010, 30, 3)->format($f));
        printf("%-18s %s setTs   %s\n", $zone, $s, (clone $d)->setTimestamp(1278250000)->format($f));
    }
}

$z = new DateTimeZone('America/New_York');
foreach (new DatePeriod(new DateTime('2010-03-13 12:00:00', $z), new DateInterval('P1D'), 3) as $d) {
    printf("period %s\n", $d->format($f));
}
foreach (new DatePeriod(new DateTime('2010-11-06 12:00:00', $z), new DateInterval('P1D'), 3) as $d) {
    printf("period %s\n", $d->format($f));
}
?>
--EXPECT--
America/New_York   2010-01-15 10:00:00 base    2010-01-15 10:00:00 -05:00 EST 0
America/New_York   2010-01-15 10:00:00 +6month 2010-07-15 10:00:00 -04:00 EDT 1
America/New_York   2010-01-15 10:00:00 +1day   2010-01-16 10:00:00 -05:00 EST 0
America/New_York   2010-01-15 10:00:00 +24hour 2010-01-16 10:00:00 -05:00 EST 0
America/New_York   2010-01-15 10:00:00 addP1M  2010-02-15 10:00:00 -05:00 EST 0
America/New_York   2010-01-15 10:00:00 subP1M  2009-12-15 10:00:00 -05:00 EST 0
America/New_York   2010-01-15 10:00:00 setDate 2010-08-03 10:00:00 -04:00 EDT 1
America/New_York   2010-01-15 10:00:00 setTime 2010-01-15 02:30:15 -05:00 EST 0
America/New_York   2010-01-15 10:00:00 setISO  2010-07-28 10:00:00 -04:00 EDT 1
America/New_York   2010-01-15 10:00:00 setTs   2010-07-04 09:26:40 -04:00 EDT 1
America/New_York   2010-03-13 12:00:00 base    2010-03-13 12:00:00 -05:00 EST 0
America/New_York   2010-03-13 12:00:00 +6month 2010-09-13 12:00:00 -04:00 EDT 1
America/New_York   2010-03-13 12:00:00 +1day   2010-03-14 12:00:00 -04:00 EDT 1
America/New_York   2010-03-13 12:00:00 +24hour 2010-03-14 12:00:00 -04:00 EDT 1
America/New_York   2010-03-13 12:00:00 addP1M  2010-04-13 12:00:00 -04:00 EDT 1
America/New_York   2010-03-13 12:00:00 subP1M  2010-02-13 12:00:00 -05:00 EST 0
America/New_York   2010-03-13 12:00:00 setDate 2010-08-03 12:00:00 -04:00 EDT 1
America/New_York   2010-03-13 12:00:00 setTime 2010-03-13 02:30:15 -05:00 EST 0
America/New_York   2010-03-13 12:00:00 setISO  2010-07-28 12:00:00 -04:00 EDT 1
America/New_York   2010-03-13 12:00:00 setTs   2010-07-04 09:26:40 -04:00 EDT 1
America/New_York   2010-11-06 12:00:00 base    2010-11-06 12:00:00 -04:00 EDT 1
America/New_York   2010-11-06 12:00:00 +6month 2011-05-06 12:00:00 -04:00 EDT 1
America/New_York   2010-11-06 12:00:00 +1day   2010-11-07 12:00:00 -05:00 EST 0
America/New_York   2010-11-06 12:00:00 +24hour 2010-11-07 12:00:00 -05:00 EST 0
America/New_York   2010-11-06 12:00:00 addP1M  2010-12-06 12:00:00 -05:00 EST 0
America/New_York   2010-11-06 12:00:00 subP1M  2010-10-06 12:00:00 -04:00 EDT 1
America/New_York   2010-11-06 12:00:00 setDate 2010-08-03 12:00:00 -04:00 EDT 1
America/New_York   2010-11-06 12:00:00 setTime 2010-11-06 02:30:15 -04:00 EDT 1
America/New_York   2010-11-06 12:00:00 setISO  2010-07-28 12:00:00 -04:00 EDT 1
America/New_York   2010-11-06 12:00:00 setTs   2010-07-04 09:26:40 -04:00 EDT 1
Europe/Paris       2010-01-15 10:00:00 base    2010-01-15 10:00:00 +01:00 CET 0
Europe/Paris       2010-01-15 10:00:00 +6month 2010-07-15 10:00:00 +02:00 CEST 1
Europe/Paris       2010-01-15 10:00:00 +1day   2010-01-16 10:00:00 +01:00 CET 0
Europe/Paris       2010-01-15 10:00:00 +24hour 2010-01-16 10:00:00 +01:00 CET 0
Europe/Paris       2010-01-15 10:00:00 addP1M  2010-02-15 10:00:00 +01:00 CET 0
Europe/Paris       2010-01-15 10:00:00 subP1M  2009-12-15 10:00:00 +01:00 CET 0
Europe/Paris       2010-01-15 10:00:00 setDate 2010-08-03 10:00:00 +02:00 CEST 1
Europe/Paris       2010-01-15 10:00:00 setTime 2010-01-15 02:30:15 +01:00 CET 0
Europe/Paris       2010-01-15 10:00:00 setISO  2010-07-28 10:00:00 +02:00 CEST 1
Europe/Paris       2010-01-15 10:00:00 setTs   2010-07-04 15:26:40 +02:00 CEST 1
Europe/Paris       2010-03-13 12:00:00 base    2010-03-13 12:00:00 +01:00 CET 0
Europe/Paris       2010-03-13 12:00:00 +6month 2010-09-13 12:00:00 +02:00 CEST 1
Europe/Paris       2010-03-13 12:00:00 +1day   2010-03-14 12:00:00 +01:00 CET 0
Europe/Paris       2010-03-13 12:00:00 +24hour 2010-03-14 12:00:00 +01:00 CET 0
Europe/Paris       2010-03-13 12:00:00 addP1M  2010-04-13 12:00:00 +02:00 CEST 1
Europe/Paris       2010-03-13 12:00:00 subP1M  2010-02-13 12:00:00 +01:00 CET 0
Europe/Paris       2010-03-13 12:00:00 setDate 2010-08-03 12:00:00 +02:00 CEST 1
Europe/Paris       2010-03-13 12:00:00 setTime 2010-03-13 02:30:15 +01:00 CET 0
Europe/Paris       2010-03-13 12:00:00 setISO  2010-07-28 12:00:00 +02:00 CEST 1
Europe/Paris       2010-03-13 12:00:00 setTs   2010-07-04 15:26:40 +02:00 CEST 1
Europe/Paris       2010-11-06 12:00:00 base    2010-11-06 12:00:00 +01:00 CET 0
Europe/Paris       2010-11-06 12:00:00 +6month 2011-05-06 12:00:00 +02:00 CEST 1
Europe/Paris       2010-11-06 12:00:00 +1day   2010-11-07 12:00:00 +01:00 CET 0
Europe/Paris       2010-11-06 12:00:00 +24hour 2010-11-07 12:00:00 +01:00 CET 0
Europe/Paris       2010-11-06 12:00:00 addP1M  2010-12-06 12:00:00 +01:00 CET 0
Europe/Paris       2010-11-06 12:00:00 subP1M  2010-10-06 12:00:00 +02:00 CEST 1
Europe/Paris       2010-11-06 12:00:00 setDate 2010-08-03 12:00:00 +02:00 CEST 1
Europe/Paris       2010-11-06 12:00:00 setTime 2010-11-06 02:30:15 +01:00 CET 0
Europe/Paris       2010-11-06 12:00:00 setISO  2010-07-28 12:00:00 +02:00 CEST 1
Europe/Paris       2010-11-06 12:00:00 setTs   2010-07-04 15:26:40 +02:00 CEST 1
Australia/Sydney   2010-01-15 10:00:00 base    2010-01-15 10:00:00 +11:00 AEDT 1
Australia/Sydney   2010-01-15 10:00:00 +6month 2010-07-15 10:00:00 +10:00 AEST 0
Australia/Sydney   2010-01-15 10:00:00 +1day   2010-01-16 10:00:00 +11:00 AEDT 1
Australia/Sydney   2010-01-15 10:00:00 +24hour 2010-01-16 10:00:00 +11:00 AEDT 1
Australia/Sydney   2010-01-15 10:00:00 addP1M  2010-02-15 10:00:00 +11:00 AEDT 1
Australia/Sydney   2010-01-15 10:00:00 subP1M  2009-12-15 10:00:00 +11:00 AEDT 1
Australia/Sydney   2010-01-15 10:00:00 setDate 2010-08-03 10:00:00 +10:00 AEST 0
Australia/Sydney   2010-01-15 10:00:00 setTime 2010-01-15 02:30:15 +11:00 AEDT 1
Australia/Sydney   2010-01-15 10:00:00 setISO  2010-07-28 10:00:00 +10:00 AEST 0
Australia/Sydney   2010-01-15 10:00:00 setTs   2010-07-04 23:26:40 +10:00 AEST 0
Australia/Sydney   2010-03-13 12:00:00 base    2010-03-13 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-03-13 12:00:00 +6month 2010-09-13 12:00:00 +10:00 AEST 0
Australia/Sydney   2010-03-13 12:00:00 +1day   2010-03-14 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-03-13 12:00:00 +24hour 2010-03-14 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-03-13 12:00:00 addP1M  2010-04-13 12:00:00 +10:00 AEST 0
Australia/Sydney   2010-03-13 12:00:00 subP1M  2010-02-13 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-03-13 12:00:00 setDate 2010-08-03 12:00:00 +10:00 AEST 0
Australia/Sydney   2010-03-13 12:00:00 setTime 2010-03-13 02:30:15 +11:00 AEDT 1
Australia/Sydney   2010-03-13 12:00:00 setISO  2010-07-28 12:00:00 +10:00 AEST 0
Australia/Sydney   2010-03-13 12:00:00 setTs   2010-07-04 23:26:40 +10:00 AEST 0
Australia/Sydney   2010-11-06 12:00:00 base    2010-11-06 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-11-06 12:00:00 +6month 2011-05-06 12:00:00 +10:00 AEST 0
Australia/Sydney   2010-11-06 12:00:00 +1day   2010-11-07 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-11-06 12:00:00 +24hour 2010-11-07 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-11-06 12:00:00 addP1M  2010-12-06 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-11-06 12:00:00 subP1M  2010-10-06 12:00:00 +11:00 AEDT 1
Australia/Sydney   2010-11-06 12:00:00 setDate 2010-08-03 12:00:00 +10:00 AEST 0
Australia/Sydney   2010-11-06 12:00:00 setTime 2010-11-06 02:30:15 +11:00 AEDT 1
Australia/Sydney   2010-11-06 12:00:00 setISO  2010-07-28 12:00:00 +10:00 AEST 0
Australia/Sydney   2010-11-06 12:00:00 setTs   2010-07-04 23:26:40 +10:00 AEST 0
period 2010-03-13 12:00:00 -05:00 EST 0
period 2010-03-14 12:00:00 -04:00 EDT 1
period 2010-03-15 12:00:00 -04:00 EDT 1
period 2010-03-16 12:00:00 -04:00 EDT 1
period 2010-11-06 12:00:00 -04:00 EDT 1
period 2010-11-07 12:00:00 -05:00 EST 0
period 2010-11-08 12:00:00 -05:00 EST 0
period 2010-11-09 12:00:00 -05:00 EST 0
