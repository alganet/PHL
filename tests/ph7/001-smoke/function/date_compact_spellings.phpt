--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php reads a bare run of digits as a date, a clock or a year
--FILE--
<?php
/* php's scanner takes a run of digits with no separator in it by WIDTH: eight
 * are a date, seven a year and a DAY OF THE YEAR, six a clock, four a clock --
 * or, when the string already named a clock, a YEAR, which is php's own dispatch
 * inside one rule. `20240102`, the most common compact date spelling there is,
 * did not parse here at all, and neither did `1234`, `123456` or a single-digit
 * hour in `1:2`.
 *
 * Only the LONGEST reading counts, which is what leaves the tail of an over-wide
 * run to the rest of the string: `12345-01-01` is 12:34 on 2005-01-01, and
 * `2020 Jan 15` is a parse failure because php's year-and-month-name token is
 * longer than the clock reading of those same four digits. */
date_default_timezone_set('UTC');

$base = strtotime('2020-06-15 12:30:45');

echo "--- by width\n";
foreach (['20240102', '2020366', '2020001', '2020367', '123456', '235959', '240000',
          '250000', '1234', '2400', '2500', '0060', '9000', '99999999', '20240102123456',
          '202401021234', '12345', '123', '1234567', '00000000', '0000000000'] as $spec) {
    $t = @strtotime($spec, $base);
    printf("%-16s %s\n", $spec, $t === false ? 'false' : date('Y-m-d H:i:s', $t));
}

echo "--- a second run is the YEAR, a third is a refusal\n";
foreach (['1234 5678', '12:00 1234', '1234 12:00', '1234 1234', '1234 2300 0000',
          '1234 5678 9012', '2020 Jan', '2020 Jan 15', '2020 15 Jan', '5678 1234',
          '12345-01-01', '1234 2020-01-01', '2020-01-01 1234'] as $spec) {
    $t = @strtotime($spec, $base);
    printf("%-18s %s\n", $spec, $t === false ? 'false' : date('Y-m-d H:i:s', $t));
}

echo "--- the t forms\n";
foreach (['t1234', 'T123456', 't9', 't24', 't25', 't12', 't1 1234', 't95846', 't7742'] as $spec) {
    $t = @strtotime($spec, $base);
    printf("%-10s %s\n", $spec, $t === false ? 'false' : date('Y-m-d H:i:s', $t));
}

echo "--- a clock field is one or two digits, greedily, and either separator\n";
foreach (['1:2', '12:3', '1:23', '12:34:5', '1:2:3', '9:9', '24:0', '24:00',
          '1.2', '12.34', '12.34.56', '1:2.5', '12:34.5', '12:34:56.5', '12:34:60',
          '0:0:0'] as $spec) {
    $t = @strtotime($spec, $base);
    printf("%-12s %s\n", $spec, $t === false ? 'false' : date('Y-m-d H:i:s', $t));
}

echo "--- and what does not parse says where\n";
foreach (['12:60', '25:00', '12:34:61', '1:60', '12:345', '12:34:56:78', '24:60',
          '12', '1 day 12', '123 456', '2020-01-01 12', '12345', '1234 12:00'] as $spec) {
    try {
        new DateTime($spec);
        printf("%-14s ok\n", $spec);
    } catch (Throwable $e) {
        printf("%-14s %s\n", $spec, preg_replace('/^.*at position/', 'pos', $e->getMessage()));
    }
}

echo "--- a relative unit still wins over every one of them\n";
foreach (['1072 month', '-2030 week 1072 month', '2020 days', '20240102 minutes',
          '1234 hours', '+1234 days'] as $spec) {
    $t = @strtotime($spec, $base);
    printf("%-24s %s\n", $spec, $t === false ? 'false' : date('Y-m-d H:i:s', $t));
}
?>
--EXPECT--
--- by width
20240102         2024-01-02 00:00:00
2020366          2020-12-31 00:00:00
2020001          2020-01-01 00:00:00
2020367          false
123456           2020-06-15 12:34:56
235959           2020-06-15 23:59:59
240000           2020-06-16 00:00:00
250000           false
1234             2020-06-15 12:34:00
2400             2020-06-16 00:00:00
2500             2500-06-15 12:30:45
0060             0060-06-15 12:30:45
9000             9000-06-15 12:30:45
99999999         9999-06-15 12:30:45
20240102123456   2024-01-02 12:34:56
202401021234     2024-01-02 12:34:00
12345            false
123              false
1234567          false
00000000         -0001-11-30 00:00:00
0000000000       false
--- a second run is the YEAR, a third is a refusal
1234 5678          5678-06-15 12:34:00
12:00 1234         1234-06-15 12:00:00
1234 12:00         false
1234 1234          1234-06-15 12:34:00
1234 2300 0000     false
1234 5678 9012     9012-06-15 12:34:00
2020 Jan           2020-01-01 00:00:00
2020 Jan 15        false
2020 15 Jan        2020-01-15 20:20:00
5678 1234          5678-06-15 12:34:00
12345-01-01        2005-01-01 12:34:00
1234 2020-01-01    2020-01-01 12:34:00
2020-01-01 1234    2020-01-01 12:34:00
--- the t forms
t1234      2020-06-15 12:34:00
T123456    2020-06-15 12:34:56
t9         2020-06-15 09:00:00
t24        2020-06-16 00:00:00
t25        false
t12        2020-06-15 12:00:00
t1 1234    1234-06-15 01:00:00
t95846     5846-06-15 09:00:00
t7742      false
--- a clock field is one or two digits, greedily, and either separator
1:2          2020-06-15 01:02:00
12:3         2020-06-15 12:03:00
1:23         2020-06-15 01:23:00
12:34:5      2020-06-15 12:34:05
1:2:3        2020-06-15 01:02:03
9:9          2020-06-15 09:09:00
24:0         2020-06-16 00:00:00
24:00        2020-06-16 00:00:00
1.2          2020-06-15 01:02:00
12.34        2020-06-15 12:34:00
12.34.56     2020-06-15 12:34:56
1:2.5        2020-06-15 01:02:05
12:34.5      2020-06-15 12:34:05
12:34:56.5   2020-06-15 12:34:56
12:34:60     2020-06-15 12:35:00
0:0:0        2020-06-15 00:00:00
--- and what does not parse says where
12:60          pos 4 (0): Unexpected character
25:00          pos 0 (2): Unexpected character
12:34:61       pos 7 (1): Unexpected character
1:60           pos 3 (0): Unexpected character
12:345         pos 5 (5): Unexpected character
12:34:56:78    pos 8 (:): Unexpected character
24:60          pos 4 (0): Unexpected character
12             pos 0 (1): Unexpected character
1 day 12       pos 6 (1): Unexpected character
123 456        pos 0 (1): Unexpected character
2020-01-01 12  pos 11 (1): Unexpected character
12345          pos 4 (5): Unexpected character
1234 12:00     pos 5 (1): Double time specification
--- a relative unit still wins over every one of them
1072 month               2109-10-15 12:30:45
-2030 week 1072 month    2070-11-18 12:30:45
2020 days                2025-12-26 12:30:45
20240102 minutes         2058-12-09 03:32:45
1234 hours               2020-08-05 22:30:45
+1234 days               2023-11-01 12:30:45
