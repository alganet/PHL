--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An ABBREVIATION beats the database, names itself, and decides what T prints
--FILE--
<?php
/* php resolves a zone name against its ABBREVIATION table BEFORE the database,
 * and ten names are in both. So `CET` is a FIXED +01:00 that never observes
 * daylight time, while the zone FILE of that name switches twice a year -- and
 * the same holds for EET, EST, GMT, HST, MET, MST, UCT, UTC and WET. Getting
 * the order backwards turns a refusal into a quietly wrong summer offset.
 *
 * An abbreviation also NAMES itself differently from an identifier: the table's
 * canonical upper-case spelling whatever the caller wrote, where an identifier
 * keeps the caller's bytes. Its daylight flag is part of the name rather than of
 * the instant -- a date in `EDT` reads I=1 forever.
 *
 * And it decides what `T` prints. php keys that specifier on the zone's TYPE:
 * an abbreviation and an identifier both print a NAME, and only a fixed OFFSET
 * is spelled "GMT+0530". PHL keyed it on whether the offset was zero, so every
 * military letter and every abbreviation with an offset printed "GMT-0700" for
 * php's "T". */
date_default_timezone_set('UTC');

foreach (['CET', 'cet', 'EET', 'WET', 'MET', 'UCT', 'EST', 'MST', 'HST', 'EDT',
          'CEST', 'BST', 'AEDT', 'T', 't', 'A', 'Z', 'GMT', 'UTC', 'utc'] as $name) {
    $z = new DateTimeZone($name);
    $summer = new DateTime('@1278250000');
    $summer->setTimezone($z);
    printf("%-6s name=%-6s type=%d P=%s T=%-9s I=%s\n", $name, $z->getName(),
        ((array) $z)['timezone_type'], $summer->format('P'), $summer->format('T'),
        $summer->format('I'));
}

/* The same five names as IDENTIFIERS are reachable through the file only when
 * nothing else claims them, which is what `Europe/Zurich` and `Etc/GMT+5` show:
 * a real database zone still switches, and a fixed OFFSET still prints GMT. */
foreach (['Europe/Zurich', 'Etc/GMT+5', '+05:30', '-03:00'] as $name) {
    $z = new DateTimeZone($name);
    $summer = new DateTime('@1278250000');
    $summer->setTimezone($z);
    printf("%-14s name=%-14s type=%d P=%s T=%-9s I=%s\n", $name, $z->getName(),
        ((array) $z)['timezone_type'], $summer->format('P'), $summer->format('T'),
        $summer->format('I'));
}
?>
--EXPECT--
CET    name=CET    type=2 P=+01:00 T=CET       I=0
cet    name=CET    type=2 P=+01:00 T=CET       I=0
EET    name=EET    type=2 P=+02:00 T=EET       I=0
WET    name=WET    type=2 P=+00:00 T=WET       I=0
MET    name=MET    type=2 P=+01:00 T=MET       I=0
UCT    name=UCT    type=2 P=+00:00 T=UCT       I=0
EST    name=EST    type=2 P=-05:00 T=EST       I=0
MST    name=MST    type=2 P=-07:00 T=MST       I=0
HST    name=HST    type=2 P=-10:00 T=HST       I=0
EDT    name=EDT    type=2 P=-04:00 T=EDT       I=1
CEST   name=CEST   type=2 P=+02:00 T=CEST      I=1
BST    name=BST    type=2 P=+01:00 T=BST       I=1
AEDT   name=AEDT   type=2 P=+11:00 T=AEDT      I=1
T      name=T      type=2 P=-07:00 T=T         I=0
t      name=T      type=2 P=-07:00 T=T         I=0
A      name=A      type=2 P=+01:00 T=A         I=0
Z      name=Z      type=2 P=+00:00 T=Z         I=0
GMT    name=GMT    type=2 P=+00:00 T=GMT       I=0
UTC    name=UTC    type=3 P=+00:00 T=UTC       I=0
utc    name=UTC    type=2 P=+00:00 T=UTC       I=0
Europe/Zurich  name=Europe/Zurich  type=3 P=+02:00 T=CEST      I=1
Etc/GMT+5      name=Etc/GMT+5      type=3 P=-05:00 T=-05       I=0
+05:30         name=+05:30         type=1 P=+05:30 T=GMT+0530  I=0
-03:00         name=-03:00         type=1 P=-03:00 T=GMT-0300  I=0
