--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's ordinal WORDS and the counted weekday
--FILE--
<?php
/* php's relative count may be spelled as a WORD -- `first` through `twelfth`,
 * with `next`, `this`, `last` and `previous` as the same rule's 1, 0 and -1 --
 * and a count may stand in front of a WEEKDAY name. Neither existed here:
 * `first day`, `second monday` and `tenth day` were an unknown TIMEZONE, `1
 * monday` and `3 mon` were `Unexpected character`, and the two spellings that
 * DID parse, `-1 monday` and `+2 monday`, read the count and threw it away --
 * a silent wrong answer, since a bare weekday name is a different day.
 *
 * The three spellings differ in what they carry: a bare name hunts with php's
 * behaviour 1 (a base day that already matches counts) and zeroes the clock; a
 * WORD moves a week per count past the first, hunts with behaviour 0 and zeroes
 * the clock; a count in DIGITS moves the same weeks, hunts with behaviour 1 and
 * leaves the clock standing.
 *
 * Under them php's longest-match tokenizer decides two more things: a weekday
 * name is a unit word's prefix (`next month` is the month, not `mon` and a
 * stray `th`), and a name standing ALONE competes with the timezone token --
 * which is the longer read of `mons` and `tues`, so those are an unknown zone
 * there while `3 mons` is the third Monday in the military zone S. */
date_default_timezone_set('UTC');
$base = '2020-01-01 10:20:30';   /* a Wednesday */
$rows = ["first day","second day","third month","fourth week","fifth year","sixth hour","seventh min",
         "eighth sec","ninth days","tenth day","eleventh day","twelfth day","thirteenth day",
         "first weeks","first week","next week","next weeks","first fortnight","second fortnight",
         "first monday","second monday","third tuesday","fourth monday","twelfth monday",
         "1 monday","2 monday","0 monday","-1 monday","+2 monday","12 monday","2monday","3 mon",
         "2 sunday","2 monday 3 tuesday","1 monday 2 days","2 monday ago","second monday ago",
         "1 weekday","first weekday","next weekday","2 weekdays ago",
         "mondays","sundays","mons","tues","thur","1 mondays","3 mons","2 thur","2 tues","1 weds",
         "2 thurs","first mons","next mons","this tues","1 mondayx","1 monx","1 satz",
         "mondayx","mondaysx","1 mondaysx","monx","sundayz","tuesdayss",
         "next month","3 months","second seconds","first minutes","2 monday 12:00"];
foreach ($rows as $s) {
    try { $d = new DateTime($base); $d->modify($s); $r = $d->format('Y-m-d H:i:s'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-20s %s\n", $s, $r);
}
/* the counted form leaves the clock where it is and a WORD zeroes it; the zone
 * a leftover letter becomes is only visible through a constructor */
foreach (["1 monday","first monday","3 mons","2 thur"] as $s) {
    $d = new DateTime("2020-01-01 10:20:30 $s");
    printf("%-14s %s %s\n", $s, $d->format('Y-m-d H:i:s'), $d->getTimezone()->getName());
}
?>
--EXPECT--
first day            2020-01-02 10:20:30
second day           2020-01-03 10:20:30
third month          2020-04-01 10:20:30
fourth week          DateTime::modify(): Failed to parse time string (fourth week) at position 0 (f): The timezone could not be found in the database
fifth year           2025-01-01 10:20:30
sixth hour           2020-01-01 16:20:30
seventh min          2020-01-01 10:27:30
eighth sec           2020-01-01 10:20:38
ninth days           2020-01-10 10:20:30
tenth day            2020-01-11 10:20:30
eleventh day         2020-01-12 10:20:30
twelfth day          2020-01-13 10:20:30
thirteenth day       DateTime::modify(): Failed to parse time string (thirteenth day) at position 0 (t): The timezone could not be found in the database
first weeks          2020-01-08 10:20:30
first week           DateTime::modify(): Failed to parse time string (first week) at position 0 (f): The timezone could not be found in the database
next week            2020-01-06 10:20:30
next weeks           2020-01-08 10:20:30
first fortnight      2020-01-15 10:20:30
second fortnight     2020-01-29 10:20:30
first monday         2020-01-06 00:00:00
second monday        2020-01-13 00:00:00
third tuesday        2020-01-21 00:00:00
fourth monday        2020-01-27 00:00:00
twelfth monday       2020-03-23 00:00:00
1 monday             2020-01-06 10:20:30
2 monday             2020-01-13 10:20:30
0 monday             2020-01-06 10:20:30
-1 monday            2019-12-30 10:20:30
+2 monday            2020-01-13 10:20:30
12 monday            2020-03-23 10:20:30
2monday              2020-01-13 10:20:30
3 mon                2020-01-20 10:20:30
2 sunday             2020-01-12 10:20:30
2 monday 3 tuesday   2020-01-28 10:20:30
1 monday 2 days      2020-01-08 10:20:30
2 monday ago         2019-12-16 10:20:30
second monday ago    2019-12-16 00:00:00
1 weekday            2020-01-02 10:20:30
first weekday        2020-01-02 00:00:00
next weekday         2020-01-02 00:00:00
2 weekdays ago       2019-12-30 10:20:30
mondays              2020-01-06 00:00:00
sundays              2020-01-05 00:00:00
mons                 DateTime::modify(): Failed to parse time string (mons) at position 0 (m): The timezone could not be found in the database
tues                 DateTime::modify(): Failed to parse time string (tues) at position 0 (t): The timezone could not be found in the database
thur                 DateTime::modify(): Failed to parse time string (thur) at position 0 (t): The timezone could not be found in the database
1 mondays            2020-01-06 10:20:30
3 mons               2020-01-20 10:20:30
2 thur               2020-01-09 10:20:30
2 tues               2020-01-14 10:20:30
1 weds               2020-01-01 10:20:30
2 thurs              DateTime::modify(): Failed to parse time string (2 thurs) at position 5 (r): The timezone could not be found in the database
first mons           2020-01-06 00:00:00
next mons            2020-01-06 00:00:00
this tues            2020-01-07 00:00:00
1 mondayx            2020-01-06 10:20:30
1 monx               2020-01-06 10:20:30
1 satz               2020-01-04 10:20:30
mondayx              2020-01-06 00:00:00
mondaysx             2020-01-06 00:00:00
1 mondaysx           2020-01-06 10:20:30
monx                 DateTime::modify(): Failed to parse time string (monx) at position 0 (m): The timezone could not be found in the database
sundayz              2020-01-05 00:00:00
tuesdayss            2020-01-07 00:00:00
next month           2020-02-01 10:20:30
3 months             2020-04-01 10:20:30
second seconds       2020-01-01 10:20:32
first minutes        2020-01-01 10:21:30
2 monday 12:00       2020-01-13 12:00:00
1 monday       2020-01-06 10:20:30 UTC
first monday   2020-01-06 00:00:00 UTC
3 mons         2020-01-20 10:20:30 S
2 thur         2020-01-09 10:20:30 R
