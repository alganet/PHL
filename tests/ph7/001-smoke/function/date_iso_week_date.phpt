--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's ISO WEEK date spelling, YYYY[-]Www[[-]D]
--FILE--
<?php
/* The ISO 8601 spelling that names a WEEK rather than a day. Every form of it
 * was `Unexpected character` here, so a program handed `2020-W05` -- what every
 * system that talks in weeks emits -- did not run at all.
 *
 * The grammar is narrow: four unsigned digits, an optional dash, an upper-case
 * W, exactly two week digits inside 01..53, and at most ONE day digit inside
 * 0..7 with its own optional dash. A digit past 7 is not part of the token,
 * which is why `2020-W05-8` is the week alone with `-8` left standing as a
 * zone offset.
 *
 * php does not resolve the week to a date: it writes January 1st of that year
 * and puts the distance on the RELATIVE day count -- and ASSIGNS it, the way
 * `yesterday` does, so a `+1 week` written before the token is discarded by it
 * while one written after moves on from it. */
date_default_timezone_set('UTC');
$rows = ["2020-W05","2020W05","2020-W05-3","2020W053","2020-W05-0","2020-W05-7","2020-W01","2020-W53",
         "2019-W01","2021-W53","1970-W01-1","0000-W01-1","2020-W05 12:00","2020-W05-3T10:00:00Z",
         "2020-W1","2020-W00","2020-W54","2020-w05","2020-W005","2020-W","W05","20200-W05","2020-W05-",
         "2020-W05-8","2020-W05-9","2020-W05-31","2020W058","2020W0530","-2020-W05-1","+2020-W05-1",
         "2020-W05 2021-01-01","2020-01-01 2020-W05","+1 week 2020-W05","2020-W05 +1 day","+1 month 2020-W05",
         "2020-W05 yesterday","2020-W05 monday","first day of 2020-W05"];
foreach ($rows as $s) {
    try { $r = (new DateTime($s))->format('Y-m-d H:i:s P'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-24s %s\n", $s, $r);
}
/* a week date names no time of day, so modify() leaves the receiver's clock */
$d = new DateTime('2000-06-15 08:09:10');
$d->modify('2020-W05');
echo "modify  ", $d->format('Y-m-d H:i:s'), "\n";
echo "strtotime ", var_export(strtotime('2020-W05 UTC'), true), "\n";
?>
--EXPECT--
2020-W05                 2020-01-27 00:00:00 +00:00
2020W05                  2020-01-27 00:00:00 +00:00
2020-W05-3               2020-01-29 00:00:00 +00:00
2020W053                 2020-01-29 00:00:00 +00:00
2020-W05-0               2020-01-26 00:00:00 +00:00
2020-W05-7               2020-02-02 00:00:00 +00:00
2020-W01                 2019-12-30 00:00:00 +00:00
2020-W53                 2020-12-28 00:00:00 +00:00
2019-W01                 2018-12-31 00:00:00 +00:00
2021-W53                 2022-01-03 00:00:00 +00:00
1970-W01-1               1969-12-29 00:00:00 +00:00
0000-W01-1               0000-01-03 00:00:00 +00:00
2020-W05 12:00           2020-01-27 12:00:00 +00:00
2020-W05-3T10:00:00Z     2020-01-29 10:00:00 +00:00
2020-W1                  Failed to parse time string (2020-W1) at position 4 (-): Unexpected character
2020-W00                 Failed to parse time string (2020-W00) at position 4 (-): Unexpected character
2020-W54                 Failed to parse time string (2020-W54) at position 4 (-): Unexpected character
2020-w05                 Failed to parse time string (2020-w05) at position 4 (-): Unexpected character
2020-W005                Failed to parse time string (2020-W005) at position 4 (-): Unexpected character
2020-W                   Failed to parse time string (2020-W) at position 4 (-): Unexpected character
W05                      Failed to parse time string (W05) at position 1 (0): Unexpected character
20200-W05                Failed to parse time string (20200-W05) at position 4 (0): Unexpected character
2020-W05-                Failed to parse time string (2020-W05-) at position 8 (-): Unexpected character
2020-W05-8               2020-01-27 00:00:00 -08:00
2020-W05-9               2020-01-27 00:00:00 -09:00
2020-W05-31              Failed to parse time string (2020-W05-31) at position 10 (1): Unexpected character
2020W058                 Failed to parse time string (2020W058) at position 7 (8): Unexpected character
2020W0530                Failed to parse time string (2020W0530) at position 8 (0): Unexpected character
-2020-W05-1              Failed to parse time string (-2020-W05-1) at position 5 (-): Unexpected character
+2020-W05-1              Failed to parse time string (+2020-W05-1) at position 5 (-): Unexpected character
2020-W05 2021-01-01      Failed to parse time string (2020-W05 2021-01-01) at position 9 (2): Double date specification
2020-01-01 2020-W05      Failed to parse time string (2020-01-01 2020-W05) at position 11 (2): Double date specification
+1 week 2020-W05         2020-01-27 00:00:00 +00:00
2020-W05 +1 day          2020-01-28 00:00:00 +00:00
+1 month 2020-W05        2020-02-27 00:00:00 +00:00
2020-W05 yesterday       2019-12-31 00:00:00 +00:00
2020-W05 monday          2020-02-01 00:00:00 +00:00
first day of 2020-W05    2020-01-01 00:00:00 +00:00
modify  2020-01-27 08:09:10
strtotime 1580083200
