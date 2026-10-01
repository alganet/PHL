--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A zone named inside a date string is the clock the whole reading is on
--FILE--
<?php
/* Every row was read off /usr/bin/php. Nothing here depends on the moment the
 * test runs: a string that leaves a field unset would be filled from `now`. */
date_default_timezone_set('UTC');

function show(string $s): void {
    try {
        $d = new DateTime($s);
        printf("%-46s %s %-30s %-5s %2d %11d\n", $s, $d->format('Y-m-d H:i:s P'),
            $d->getTimezone()->getName(), $d->format('T'),
            ((array) $d)['timezone_type'], $d->getTimestamp());
    } catch (Throwable $e) {
        printf("%-46s %s: %s\n", $s, get_class($e), $e->getMessage());
    }
}

echo "-- an IDENTIFIER carries the zone, not a fixed offset\n";
show('2026-01-02 03:04:05 America/New_York');
show('2026-07-02 03:04:05 America/New_York');
show('2026-01-02 03:04:05 Europe/Paris');
show('2026-01-02 03:04:05 America/Argentina/Buenos_Aires');
show('2026-01-02 03:04:05 Antarctica/DumontDUrville');
show('2026-01-02 03:04:05 Pacific/Auckland');
show('2026-01-02 03:04:05 Zulu');
show('Europe/Lisbon 2026-01-02 03:04:05');
show('2026-01-02 03:04:05 (Europe/Paris)');

echo "\n-- an ABBREVIATION beats the database, and never observes daylight time\n";
show('2026-01-02 03:04:05 EST');
show('2026-07-02 03:04:05 EST');
show('2026-01-02 03:04:05 CET');
show('2026-07-02 03:04:05 CET');
show('2026-01-02 03:04:05 pdt');

echo "\n-- only the FIRST zone decides; a second is dropped, a third refused\n";
show('2026-01-02 03:04:05 America/New_York +02:00');
show('2026-01-02 03:04:05 +02:00 America/New_York');

echo "\n-- the shapes php's scanner will not read as an identifier\n";
show('2026-01-02 03:04:05 america/new_york');
show('2026-01-02 03:04:05 US/Eastern');
show('2026-01-02 03:04:05 Foo/Bar');
show('2026-01-02 03:04:05 Etc/GMT+5');

echo "\n-- a SKIPPED hour, and a REPEATED one: the string door takes the first\n";
show('2026-03-08 02:30:00 America/New_York');
show('2026-03-29 02:30:00 Europe/Paris');
show('2026-11-01 01:30:00 America/New_York');
show('2026-10-25 02:30:00 Europe/Paris');
show('2026-04-05 02:30:00 Australia/Sydney');
show('2026-04-05 03:15:00 Pacific/Chatham');

echo "\n-- the same name read by a FORMAT\n";
foreach ([['Y-m-d H:i:s e', '2026-01-02 03:04:05 Europe/Paris'],
          ['Y-m-d H:i:s e', '2026-07-02 03:04:05 Europe/Paris'],
          ['Y-m-d H:i:s e', '2026-10-25 02:30:00 Europe/Paris'],
          ['Y-m-d H:i:s T', '2026-07-02 03:04:05 EST']] as [$f, $s]) {
    $d = DateTime::createFromFormat($f, $s);
    printf("%-14s %-38s %s\n", $f, $s, $d ? $d->format('Y-m-d H:i:s P e U') : 'false');
}

echo "\n-- strtotime() and date_parse() read the same token\n";
echo var_export(strtotime('2026-06-02 03:04:05 Europe/Paris'), true), "\n";
$p = date_parse('2026-01-02 03:04:05 America/New_York');
echo var_export($p['zone_type'], true), ' ', var_export($p['tz_id'], true), "\n";

echo "\n-- @epoch is already an instant, and modify() discards the zone\n";
show('@86400 Europe/Paris');
$m = new DateTime('2026-01-02 03:04:05');
$m->modify('America/New_York');
echo $m->format('Y-m-d H:i:s P e'), "\n";
--EXPECT--
-- an IDENTIFIER carries the zone, not a fixed offset
2026-01-02 03:04:05 America/New_York           2026-01-02 03:04:05 -05:00 America/New_York               EST    3  1767341045
2026-07-02 03:04:05 America/New_York           2026-07-02 03:04:05 -04:00 America/New_York               EDT    3  1782975845
2026-01-02 03:04:05 Europe/Paris               2026-01-02 03:04:05 +01:00 Europe/Paris                   CET    3  1767319445
2026-01-02 03:04:05 America/Argentina/Buenos_Aires 2026-01-02 03:04:05 -03:00 America/Argentina/Buenos_Aires -03    3  1767333845
2026-01-02 03:04:05 Antarctica/DumontDUrville  2026-01-02 03:04:05 +10:00 Antarctica/DumontDUrville      +10    3  1767287045
2026-01-02 03:04:05 Pacific/Auckland           2026-01-02 03:04:05 +13:00 Pacific/Auckland               NZDT   3  1767276245
2026-01-02 03:04:05 Zulu                       2026-01-02 03:04:05 +00:00 Zulu                           UTC    3  1767323045
Europe/Lisbon 2026-01-02 03:04:05              2026-01-02 03:04:05 +00:00 Europe/Lisbon                  WET    3  1767323045
2026-01-02 03:04:05 (Europe/Paris)             DateMalformedStringException: Failed to parse time string (2026-01-02 03:04:05 (Europe/Paris)) at position 20 ((): The timezone could not be found in the database

-- an ABBREVIATION beats the database, and never observes daylight time
2026-01-02 03:04:05 EST                        2026-01-02 03:04:05 -05:00 EST                            EST    2  1767341045
2026-07-02 03:04:05 EST                        2026-07-02 03:04:05 -05:00 EST                            EST    2  1782979445
2026-01-02 03:04:05 CET                        2026-01-02 03:04:05 +01:00 CET                            CET    2  1767319445
2026-07-02 03:04:05 CET                        2026-07-02 03:04:05 +01:00 CET                            CET    2  1782957845
2026-01-02 03:04:05 pdt                        2026-01-02 03:04:05 -07:00 PDT                            PDT    2  1767348245

-- only the FIRST zone decides; a second is dropped, a third refused
2026-01-02 03:04:05 America/New_York +02:00    2026-01-02 03:04:05 -05:00 America/New_York               EST    3  1767341045
2026-01-02 03:04:05 +02:00 America/New_York    2026-01-02 03:04:05 +02:00 +02:00                         GMT+0200  1  1767315845

-- the shapes php's scanner will not read as an identifier
2026-01-02 03:04:05 america/new_york           DateMalformedStringException: Failed to parse time string (2026-01-02 03:04:05 america/new_york) at position 20 (a): The timezone could not be found in the database
2026-01-02 03:04:05 US/Eastern                 DateMalformedStringException: Failed to parse time string (2026-01-02 03:04:05 US/Eastern) at position 20 (U): The timezone could not be found in the database
2026-01-02 03:04:05 Foo/Bar                    DateMalformedStringException: Failed to parse time string (2026-01-02 03:04:05 Foo/Bar) at position 20 (F): The timezone could not be found in the database
2026-01-02 03:04:05 Etc/GMT+5                  2026-01-02 03:04:05 +00:00 Etc/GMT                        GMT    3  1767323045

-- a SKIPPED hour, and a REPEATED one: the string door takes the first
2026-03-08 02:30:00 America/New_York           2026-03-08 03:30:00 -04:00 America/New_York               EDT    3  1772955000
2026-03-29 02:30:00 Europe/Paris               2026-03-29 03:30:00 +02:00 Europe/Paris                   CEST   3  1774747800
2026-11-01 01:30:00 America/New_York           2026-11-01 01:30:00 -04:00 America/New_York               EDT    3  1793511000
2026-10-25 02:30:00 Europe/Paris               2026-10-25 02:30:00 +02:00 Europe/Paris                   CEST   3  1792888200
2026-04-05 02:30:00 Australia/Sydney           2026-04-05 02:30:00 +11:00 Australia/Sydney               AEDT   3  1775316600
2026-04-05 03:15:00 Pacific/Chatham            2026-04-05 03:15:00 +13:45 Pacific/Chatham                +1345  3  1775309400

-- the same name read by a FORMAT
Y-m-d H:i:s e  2026-01-02 03:04:05 Europe/Paris       2026-01-02 03:04:05 +01:00 Europe/Paris 1767319445
Y-m-d H:i:s e  2026-07-02 03:04:05 Europe/Paris       2026-07-02 03:04:05 +02:00 Europe/Paris 1782954245
Y-m-d H:i:s e  2026-10-25 02:30:00 Europe/Paris       2026-10-25 02:30:00 +02:00 Europe/Paris 1792888200
Y-m-d H:i:s T  2026-07-02 03:04:05 EST                2026-07-02 03:04:05 -05:00 EST 1782979445

-- strtotime() and date_parse() read the same token
1780362245
3 'America/New_York'

-- @epoch is already an instant, and modify() discards the zone
@86400 Europe/Paris                            1970-01-02 00:00:00 +00:00 +00:00                         GMT+0000  1       86400
2026-01-02 03:04:05 +00:00 UTC
