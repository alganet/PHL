--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's American date with no year at all (month/day)
--FILE--
<?php
/* `4/20` -- the way an American program spells a date without a year -- did not
 * parse here at all: PHL had php's `month/day/year` and not the two-field form
 * under it, so `12/31`, `10/2` and `4/20 12:00` were all an unexpected `4`.
 *
 * php spells the fields INSIDE the pattern rather than range-checking them
 * afterwards (`"0"? [0-9] | "1"[0-2]` and `[0-2]?[0-9] | "3"[01]`), so a
 * two-digit reading that is out of range leaves its second digit to the string:
 * `4/32` is the 3rd with a stray `2` behind it, `13/20` an unexpected `1` and
 * then March the 20th. Zero matches both fields and normalizes, which makes
 * `0/1` December of the year before and `1/0` the last day of one. The ordinal
 * suffix rides the day with NOTHING between them, so `4/20th` is the 20th where
 * `4/20 th` is the 20th beside a zone php cannot find. And the YEAR is left
 * alone -- php's action writes only the month and the day. */
date_default_timezone_set('UTC');
$rows = ['4/20', '12/31', '10/2', '1/2', '04/05', '0/1', '00/1', '1/0', '0/0',
         '13/20', '4/32', '4/', '/4', '4/20/', '4/20st', '4/20th', '4/20nd',
         '4/20rd', '4/20xy', '4/2 nd', '4/20 th', '4/20 12:00', '4/2012:00',
         '4/20/2020', '2020/1/2', '2020-01-02 4/20', '4/20 4/21', '4/20 UTC'];
$base = '2020-06-15 08:09:10';
foreach ($rows as $s) {
    try { $d = new DateTime($base); $d->modify($s); $r = $d->format('Y-m-d H:i:s P'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-18s %s\n", $s, $r);
}
?>
--EXPECT--
4/20               2020-04-20 08:09:10 +00:00
12/31              2020-12-31 08:09:10 +00:00
10/2               2020-10-02 08:09:10 +00:00
1/2                2020-01-02 08:09:10 +00:00
04/05              2020-04-05 08:09:10 +00:00
0/1                2019-12-01 08:09:10 +00:00
00/1               2019-12-01 08:09:10 +00:00
1/0                2019-12-31 08:09:10 +00:00
0/0                2019-11-30 08:09:10 +00:00
13/20              DateTime::modify(): Failed to parse time string (13/20) at position 0 (1): Unexpected character
4/32               DateTime::modify(): Failed to parse time string (4/32) at position 3 (2): Unexpected character
4/                 DateTime::modify(): Failed to parse time string (4/) at position 0 (4): Unexpected character
/4                 DateTime::modify(): Failed to parse time string (/4) at position 0 (/): Unexpected character
4/20/              DateTime::modify(): Failed to parse time string (4/20/) at position 4 (/): Unexpected character
4/20st             2020-04-20 08:09:10 +00:00
4/20th             2020-04-20 08:09:10 +00:00
4/20nd             2020-04-20 08:09:10 +00:00
4/20rd             2020-04-20 08:09:10 +00:00
4/20xy             DateTime::modify(): Failed to parse time string (4/20xy) at position 4 (x): The timezone could not be found in the database
4/2 nd             DateTime::modify(): Failed to parse time string (4/2 nd) at position 4 (n): The timezone could not be found in the database
4/20 th            DateTime::modify(): Failed to parse time string (4/20 th) at position 5 (t): The timezone could not be found in the database
4/20 12:00         2020-04-20 12:00:00 +00:00
4/2012:00          2020-04-20 12:00:00 +00:00
4/20/2020          2020-04-20 08:09:10 +00:00
2020/1/2           2020-01-02 08:09:10 +00:00
2020-01-02 4/20    DateTime::modify(): Failed to parse time string (2020-01-02 4/20) at position 11 (4): Double date specification
4/20 4/21          DateTime::modify(): Failed to parse time string (4/20 4/21) at position 5 (4): Double date specification
4/20 UTC           2020-04-20 08:09:10 +00:00
