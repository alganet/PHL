--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's ordinal suffix rides the DAY of a numeric date, and only the day
--FILE--
<?php
/* php spells a day as `([0-2]?[0-9] | "3"[01]) daysuf?` and uses that one
 * nonterminal wherever a day stands, so `2020-01-02nd`, `20th-1-2020`,
 * `20th.1.2020`, `2020/1/2nd` and `4/20th/2020` are all ordinary dates there.
 * PHL read a suffix only in the TEXTUAL-month spellings (`15th january`), so
 * every numeric one refused on the `n` or the `t`.
 *
 * Which FIELD the day is depends on the separator and the widths, and a suffix
 * anywhere else is not the token at all: `2020th-1-2` and `2020/1th/2` are
 * refusals in both engines because the separator behind the suffix never
 * matches. A suffix on a field the mapping makes the YEAR is simply not read,
 * so `20-1-2020th` is the 20th of January with `th` left to the string as an
 * unknown zone -- and a run wider than two digits is no day at all, which is
 * what refuses `020th-1-2020` on its first byte.
 *
 * The American form has a quirk of its own: php reads its year through a helper
 * that comes back UNSET once the suffix has been stepped over, so `4/20th/2020`
 * keeps the BASE moment's year where `4/20/2020` takes 2020's. Only the plain
 * four-digit ISO year takes a suffix behind it -- `+12345-01-02nd` leaves the
 * `nd` to the string. */
date_default_timezone_set('UTC');
$rows = ['2020-01-02nd', '2020-1-2nd', '2020th-1-2', '20th-1-2020', '20-1th-2020',
         '20-1-2020th', '20th.1.2020', '20.1th.2020', '20.1.2020th', '2020/1/2nd',
         '2020th/1/2', '2020/1th/2', '4/20th/2020', '4/2nd/2020', '4/20/2020th',
         '2020-01th-02', '2020th-01-02', '020th-1-2020', '67th.3.20', '20.3.67th',
         '4/20th', '2020-01-02nd 12:00', '2020-01-02ndT12:00', '+12345-01-02nd',
         '2020-01nd', '2020-102nd', '15th january 2020', 'january 15th 2020'];
$base = '2019-06-15 08:09:10';
foreach ($rows as $s) {
    try { $d = new DateTime($base); $d->modify($s); $r = $d->format('Y-m-d H:i:s'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-22s %s\n", $s, $r);
}
?>
--EXPECT--
2020-01-02nd           2020-01-02 08:09:10
2020-1-2nd             2020-01-02 08:09:10
2020th-1-2             DateTime::modify(): Failed to parse time string (2020th-1-2) at position 4 (t): The timezone could not be found in the database
20th-1-2020            2020-01-20 08:09:10
20-1th-2020            DateTime::modify(): Failed to parse time string (20-1th-2020) at position 0 (2): Unexpected character
20-1-2020th            DateTime::modify(): Failed to parse time string (20-1-2020th) at position 9 (t): The timezone could not be found in the database
20th.1.2020            2020-01-20 08:09:10
20.1th.2020            DateTime::modify(): Failed to parse time string (20.1th.2020) at position 4 (t): The timezone could not be found in the database
20.1.2020th            DateTime::modify(): Failed to parse time string (20.1.2020th) at position 9 (t): The timezone could not be found in the database
2020/1/2nd             2020-01-02 08:09:10
2020th/1/2             DateTime::modify(): Failed to parse time string (2020th/1/2) at position 4 (t): The timezone could not be found in the database
2020/1th/2             DateTime::modify(): Failed to parse time string (2020/1th/2) at position 4 (/): Unexpected character
4/20th/2020            2019-04-20 08:09:10
4/2nd/2020             2019-04-02 08:09:10
4/20/2020th            DateTime::modify(): Failed to parse time string (4/20/2020th) at position 9 (t): The timezone could not be found in the database
2020-01th-02           DateTime::modify(): Failed to parse time string (2020-01th-02) at position 7 (t): The timezone could not be found in the database
2020th-01-02           DateTime::modify(): Failed to parse time string (2020th-01-02) at position 4 (t): The timezone could not be found in the database
020th-1-2020           DateTime::modify(): Failed to parse time string (020th-1-2020) at position 0 (0): Unexpected character
67th.3.20              DateTime::modify(): Failed to parse time string (67th.3.20) at position 0 (6): Unexpected character
20.3.67th              DateTime::modify(): Failed to parse time string (20.3.67th) at position 7 (t): The timezone could not be found in the database
4/20th                 2019-04-20 08:09:10
2020-01-02nd 12:00     2020-01-02 12:00:00
2020-01-02ndT12:00     2020-01-02 12:00:00
+12345-01-02nd         DateTime::modify(): Failed to parse time string (+12345-01-02nd) at position 12 (n): The timezone could not be found in the database
2020-01nd              DateTime::modify(): Failed to parse time string (2020-01nd) at position 7 (n): The timezone could not be found in the database
2020-102nd             DateTime::modify(): Failed to parse time string (2020-102nd) at position 8 (n): The timezone could not be found in the database
15th january 2020      2020-01-15 08:09:10
january 15th 2020      2020-01-15 08:09:10
