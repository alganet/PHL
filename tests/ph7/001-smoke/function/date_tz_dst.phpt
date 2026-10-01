--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A wall clock across a daylight switch: the skipped hour and the repeated one
--FILE--
<?php
/* A wall-clock reading is not an instant when a zone observes daylight saving:
 * an hour the clock SKIPS names none, and an hour it repeats names two. php
 * settles both by preferring the offset in force BEFORE the switch, so a
 * skipped 02:30 comes back as 03:30 on the new offset and a repeated 01:30 is
 * the FIRST of its two instants. The reported offset is then the one at the
 * instant it landed on, not the one the clock was read with.
 *
 * The southern hemisphere is the same rule with the year's ends swapped, which
 * is why Sydney is here beside New York. */
date_default_timezone_set('UTC');

foreach (['America/New_York' => ['2010-03-14 01:30:00', '2010-03-14 02:30:00',
                                 '2010-03-14 03:30:00', '2010-11-07 00:30:00',
                                 '2010-11-07 01:30:00', '2010-11-07 02:30:00'],
          'Europe/Paris'     => ['2010-03-28 01:30:00', '2010-03-28 02:30:00',
                                 '2010-10-31 02:30:00', '2010-10-31 03:30:00'],
          'Australia/Sydney' => ['2010-04-04 02:30:00', '2010-10-03 02:30:00']] as $zone => $stamps) {
    $z = new DateTimeZone($zone);
    foreach ($stamps as $s) {
        $d = new DateTime($s, $z);
        printf("%-18s %s -> %s U=%d T=%-5s I=%s\n", $zone, $s,
            $d->format('Y-m-d H:i:s P'), $d->getTimestamp(), $d->format('T'), $d->format('I'));
    }
}
?>
--EXPECT--
America/New_York   2010-03-14 01:30:00 -> 2010-03-14 01:30:00 -05:00 U=1268548200 T=EST   I=0
America/New_York   2010-03-14 02:30:00 -> 2010-03-14 03:30:00 -04:00 U=1268551800 T=EDT   I=1
America/New_York   2010-03-14 03:30:00 -> 2010-03-14 03:30:00 -04:00 U=1268551800 T=EDT   I=1
America/New_York   2010-11-07 00:30:00 -> 2010-11-07 00:30:00 -04:00 U=1289104200 T=EDT   I=1
America/New_York   2010-11-07 01:30:00 -> 2010-11-07 01:30:00 -04:00 U=1289107800 T=EDT   I=1
America/New_York   2010-11-07 02:30:00 -> 2010-11-07 02:30:00 -05:00 U=1289115000 T=EST   I=0
Europe/Paris       2010-03-28 01:30:00 -> 2010-03-28 01:30:00 +01:00 U=1269736200 T=CET   I=0
Europe/Paris       2010-03-28 02:30:00 -> 2010-03-28 03:30:00 +02:00 U=1269739800 T=CEST  I=1
Europe/Paris       2010-10-31 02:30:00 -> 2010-10-31 02:30:00 +01:00 U=1288488600 T=CET   I=0
Europe/Paris       2010-10-31 03:30:00 -> 2010-10-31 03:30:00 +01:00 U=1288492200 T=CET   I=0
Australia/Sydney   2010-04-04 02:30:00 -> 2010-04-04 02:30:00 +10:00 U=1270312200 T=AEST  I=0
Australia/Sydney   2010-10-03 02:30:00 -> 2010-10-03 03:30:00 +11:00 U=1286037000 T=AEDT  I=1
