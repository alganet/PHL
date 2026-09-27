--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's YEAR-MONTH date and the ISO ordinal behind the same dash
--FILE--
<?php
/* Two shorter spellings stand behind php's `YYYY-MM-DD`, both only after a plain
 * four-digit year: the YEAR-MONTH `2020-01`, whose day is the 1st, and the ISO
 * ORDINAL `2020-102`, whose three digits are the day of the YEAR (April 11th).
 * Neither parsed here -- `2020-01` was the clock 20:20 with a `-01` zone behind
 * it, an answer no part of which the string said.
 *
 * The LONGEST reading that validates wins and the rest of the digit run is left
 * to the string, which is where php's refusals for this shape really come from:
 * `2020-13` is the month 1 with a stray `3` after it, `1526-797-45` the month 7
 * with `97-45` left over, and `2020-000` the month 0 with a `0`. The same rule
 * spells the DAY of the full form: `3854-2-40` is the 4th with a `0` after it,
 * not the year-month `3854-2` with a `-40` timezone behind it. */
date_default_timezone_set('UTC');

$base = 1592222222;   /* 2020-06-15 12:37:02 UTC */

echo "--- year-month\n";
foreach (['2020-01', '2020-1', '2020-10', '2020-12', '2020-00', '2020-0', '2020-07 12:00',
          '2020-07T12:00', '2020-01 UTC', '2020-01Z', '2020-01+02:00', '12:00 2020-01',
          '2020-01 12:00'] as $spec) {
    $t = strtotime($spec, $base);
    printf("%-18s %s\n", $spec, $t === false ? 'REFUSED' : date('Y-m-d H:i:s', $t));
}

echo "--- the ISO ordinal\n";
foreach (['2020-001', '2020-012', '2020-100', '2020-102', '2020-366', '2021-366',
          '2020-012 12:00', '2020-360'] as $spec) {
    $t = strtotime($spec, $base);
    printf("%-18s %s\n", $spec, $t === false ? 'REFUSED' : date('Y-m-d H:i:s', $t));
}

echo "--- what the longest valid reading leaves behind\n";
foreach (['2020-13', '2020-99', '2020-000', '2020-367', '2020-400', '2020-0112',
          '9313-48180', '1526-79', '20-1', '202-1', '+12345-01', '2020-1-', '2020-01-'] as $spec) {
    $t = strtotime($spec, $base);
    printf("%-18s %s\n", $spec, $t === false ? 'REFUSED' : date('Y-m-d H:i:s', $t));
}

echo "--- the full form still wins, and spells its day the same way\n";
foreach (['2020-01-01', '2020-1-1', '2020-1-1 12:00', '2020-12-31 23:59:59', '3854-2-4',
          '3854-2-40', '3854-2-99', '3854-12-40', '2020-13-01', '2020-12-32'] as $spec) {
    $t = strtotime($spec, $base);
    printf("%-20s %s\n", $spec, $t === false ? 'REFUSED' : date('Y-m-d H:i:s', $t));
}
foreach (['2020-13', '1526-797-45', '3854-2-40'] as $spec) {
    try {
        new DateTime($spec);
        printf("%-14s parsed\n", $spec);
    } catch (Throwable $e) {
        printf("%-14s %s\n", $spec, $e->getMessage());
    }
}
?>
--EXPECT--
--- year-month
2020-01            2020-01-01 00:00:00
2020-1             2020-01-01 00:00:00
2020-10            2020-10-01 00:00:00
2020-12            2020-12-01 00:00:00
2020-00            2019-12-01 00:00:00
2020-0             2019-12-01 00:00:00
2020-07 12:00      2020-07-01 12:00:00
2020-07T12:00      2020-07-01 12:00:00
2020-01 UTC        2020-01-01 00:00:00
2020-01Z           2020-01-01 00:00:00
2020-01+02:00      2019-12-31 22:00:00
12:00 2020-01      2020-01-01 12:00:00
2020-01 12:00      2020-01-01 12:00:00
--- the ISO ordinal
2020-001           2020-01-01 00:00:00
2020-012           2020-01-12 00:00:00
2020-100           2020-04-09 00:00:00
2020-102           2020-04-11 00:00:00
2020-366           2020-12-31 00:00:00
2021-366           2022-01-01 00:00:00
2020-012 12:00     2020-01-12 12:00:00
2020-360           2020-12-25 00:00:00
--- what the longest valid reading leaves behind
2020-13            REFUSED
2020-99            REFUSED
2020-000           REFUSED
2020-367           REFUSED
2020-400           REFUSED
2020-0112          REFUSED
9313-48180         8180-04-01 00:00:00
1526-79            REFUSED
20-1               REFUSED
202-1              REFUSED
+12345-01          REFUSED
2020-1-            REFUSED
2020-01-           REFUSED
--- the full form still wins, and spells its day the same way
2020-01-01           2020-01-01 00:00:00
2020-1-1             2020-01-01 00:00:00
2020-1-1 12:00       2020-01-01 12:00:00
2020-12-31 23:59:59  2020-12-31 23:59:59
3854-2-4             3854-02-04 00:00:00
3854-2-40            REFUSED
3854-2-99            REFUSED
3854-12-40           REFUSED
2020-13-01           REFUSED
2020-12-32           REFUSED
2020-13        Failed to parse time string (2020-13) at position 6 (3): Unexpected character
1526-797-45    Failed to parse time string (1526-797-45) at position 6 (9): Unexpected character
3854-2-40      Failed to parse time string (3854-2-40) at position 8 (0): Unexpected character
