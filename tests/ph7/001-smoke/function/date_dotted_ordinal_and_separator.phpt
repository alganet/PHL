--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's dotted ISO ordinal date, and the full stop as an ordinary separator
--FILE--
<?php
/* php reads its ISO ORDINAL date with a FULL STOP as well as a dash --
 * `2020.102` is the same 11th of April `2020-102` gives -- and it is the ONLY
 * dotted form a four-digit year takes: `2020.1` and `2020.12` are no date at
 * all (the four digits are a clock and the rest is refused), the run is exactly
 * three digits inside 001..366, and the sign belongs to a rule of its own.
 *
 * PHL had no such rule, and stood the COMPETITION down instead: a full stop
 * before a digit was simply not a separator, so every string where no dotted
 * date is there to claim it refused on the dot. php's separator run steps over
 * it unconditionally -- the rules that spell a dot claim their own bytes first
 * -- so `20240102.2020` is a date, a separator and a clock, and `1234.2020` is
 * this ordinal date with a digit left over, refused on the fifth byte of the
 * run rather than on the dot. */
date_default_timezone_set('UTC');
$rows = ['2020.102', '2020-102', '1234.202', '2020.366', '2020.001', '2020.000',
         '2020.400', '2020.1', '2020.12', '2020.123', '2020.1234', '+2020.102',
         '2020/102', '2020 102', '2020.102T12:00', '2020.102 12:00', '2020.102Z',
         '2020.102.3', '1234.2020', '2020.2020', '5218.1268', '1234.20', '1234.2',
         '20240102.2020', '1.2.2020', '1.2.20200', '20.3.67', '12:00.5',
         '12:00.2020', '12:00.UTC', '+1 day.+2 hours', '3pm,'];
$base = '2019-06-15 08:09:10';
foreach ($rows as $s) {
    try { $d = new DateTime($base); $d->modify($s); $r = $d->format('Y-m-d H:i:s'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-18s %s\n", $s, $r);
}
?>
--EXPECT--
2020.102           2020-04-11 08:09:10
2020-102           2020-04-11 08:09:10
1234.202           1234-07-21 08:09:10
2020.366           2020-12-31 08:09:10
2020.001           2020-01-01 08:09:10
2020.000           DateTime::modify(): Failed to parse time string (2020.000) at position 5 (0): Unexpected character
2020.400           DateTime::modify(): Failed to parse time string (2020.400) at position 5 (4): Unexpected character
2020.1             DateTime::modify(): Failed to parse time string (2020.1) at position 5 (1): Unexpected character
2020.12            DateTime::modify(): Failed to parse time string (2020.12) at position 5 (1): Unexpected character
2020.123           2020-05-02 08:09:10
2020.1234          DateTime::modify(): Failed to parse time string (2020.1234) at position 8 (4): Unexpected character
+2020.102          DateTime::modify(): Failed to parse time string (+2020.102) at position 6 (1): Unexpected character
2020/102           DateTime::modify(): Failed to parse time string (2020/102) at position 4 (/): Unexpected character
2020 102           DateTime::modify(): Failed to parse time string (2020 102) at position 5 (1): Unexpected character
2020.102T12:00     2020-04-11 12:00:00
2020.102 12:00     2020-04-11 12:00:00
2020.102Z          2020-04-11 08:09:10
2020.102.3         DateTime::modify(): Failed to parse time string (2020.102.3) at position 9 (3): Unexpected character
1234.2020          DateTime::modify(): Failed to parse time string (1234.2020) at position 8 (0): Unexpected character
2020.2020          DateTime::modify(): Failed to parse time string (2020.2020) at position 8 (0): Unexpected character
5218.1268          DateTime::modify(): Failed to parse time string (5218.1268) at position 8 (8): Unexpected character
1234.20            DateTime::modify(): Failed to parse time string (1234.20) at position 5 (2): Unexpected character
1234.2             DateTime::modify(): Failed to parse time string (1234.2) at position 5 (2): Unexpected character
20240102.2020      2024-01-02 20:20:00
1.2.2020           2020-02-01 08:09:10
1.2.20200          DateTime::modify(): Failed to parse time string (1.2.20200) at position 8 (0): Unexpected character
20.3.67            2067-03-20 08:09:10
12:00.5            2019-06-15 12:00:05
12:00.2020         DateTime::modify(): Failed to parse time string (12:00.2020) at position 8 (2): Unexpected character
12:00.UTC          2019-06-15 12:00:00
+1 day.+2 hours    2019-06-16 10:09:10
3pm,               DateTime::modify(): Failed to parse time string (3pm,) at position 0 (3): Unexpected character
