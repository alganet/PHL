--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's dotted date takes a two-digit year, and the clock takes it back
--FILE--
<?php
/* php's dot date is day-first with a TWO- or four-digit year, so `20.03.67` is
 * 2067-03-20 (its 00-69 / 70-99 mapping) -- a spelling PHL refused outright,
 * since it read the four-digit form and nothing else.
 *
 * The two-digit form is the same BYTES as php's dotted CLOCK, and the clock is
 * the rule php's scanner declares first, so the clock wins whenever it READS:
 * `20.03.00` is the time 20:03:00 while `20.03.67`, whose seconds no clock can
 * hold, is the date. `13.01.99` is the date for the same reason (99 seconds),
 * `29.02.20` because 29 is no hour, and `12.31.99` is neither -- no clock holds
 * 99 seconds and no date has a month 31.
 *
 * The year is four digits or two, never three: the year of `20.03.671` is 67 and
 * the `1` is left to the string. */
date_default_timezone_set('UTC');

/* A fixed base, so the rows php reads as a CLOCK (whose date is "today") pin. */
$base = 1592222222;   /* 2020-06-15 12:37:02 UTC */

foreach (['20.03.67', '20.3.67', '2.3.67', '20.03.1967', '20.3.1967', '20.03.0671',
          '20.03.69', '20.03.70', '20.03.99', '31.12.99', '13.01.99', '29.02.20',
          '30.02.20', '20.03.67 12:00', '20.03.67 12:00:00',
          '20.03.00', '20.03.9', '20.03.6', '2.3.4', '1.2.3', '1.1.1', '12.34', '1.2',
          '12.31.99', '20.03.067', '20.03.671', '2.3.671', '20.03.196', '20.03.12345',
          '20.03.6712', '29.02.2', '2020.03.04'] as $spec) {
    $t = strtotime($spec, $base);
    printf("%-20s %s\n", $spec, $t === false ? 'REFUSED' : date('Y-m-d H:i:s', $t));
}
?>
--EXPECT--
20.03.67             2067-03-20 00:00:00
20.3.67              2067-03-20 00:00:00
2.3.67               2067-03-02 00:00:00
20.03.1967           1967-03-20 00:00:00
20.3.1967            1967-03-20 00:00:00
20.03.0671           0671-03-20 00:00:00
20.03.69             2069-03-20 00:00:00
20.03.70             1970-03-20 00:00:00
20.03.99             1999-03-20 00:00:00
31.12.99             1999-12-31 00:00:00
13.01.99             1999-01-13 00:00:00
29.02.20             2020-02-29 00:00:00
30.02.20             2020-03-01 00:00:00
20.03.67 12:00       2067-03-20 12:00:00
20.03.67 12:00:00    2067-03-20 12:00:00
20.03.00             2020-06-15 20:03:00
20.03.9              2020-06-15 20:03:09
20.03.6              2020-06-15 20:03:06
2.3.4                2020-06-15 02:03:04
1.2.3                2020-06-15 01:02:03
1.1.1                2020-06-15 01:01:01
12.34                2020-06-15 12:34:00
1.2                  2020-06-15 01:02:00
12.31.99             REFUSED
20.03.067            REFUSED
20.03.671            REFUSED
2.3.671              REFUSED
20.03.196            REFUSED
20.03.12345          REFUSED
20.03.6712           6712-03-20 00:00:00
29.02.2              REFUSED
2020.03.04           REFUSED
