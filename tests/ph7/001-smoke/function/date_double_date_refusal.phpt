--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A second absolute date in one string is php's refusal
--FILE--
<?php
/* php refuses a SECOND absolute date outright, at the offending token's start:
 * `2020-01-01 january` is "Double date specification" there and answered the
 * january here, silently rebuilding the date the string had already given. Every
 * date spelling counts -- ISO, slashed, dotted, a month name, and the compact
 * eight-digit run -- and the bare four-digit YEAR does not, which is what keeps
 * `1234 5678` a clock beside a year. A time of day beside a date is no
 * conflict at all. */
date_default_timezone_set('UTC');

$base = strtotime('2020-06-15 12:30:45');   /* a fixed base: half of these rows
                                             * take their month or day from it */
foreach (['2020-01-01 january', 'january january', '2020-01-01 2020-01-02',
          '20240102 20240102', '12:00 20240102', '2020-01-01 20240102',
          'first day of january january', 'first day of january last day of february 2021',
          '15 january 2020 march', '01/02/2020 03.04.2021', '2020-01-01 2020',
          '1234 5678', '2020 Jan', '2020-01-01 12:00', '2020-01-01 1234',
          'january +1 day', 'first day of january +1 month'] as $spec) {
    $t = @strtotime($spec, $base);
    if ($t !== false) {
        printf("%-46s %s\n", "'$spec'", date('Y-m-d H:i:s', $t));
        continue;
    }
    try {
        new DateTime($spec);
    } catch (Throwable $e) {
        printf("%-46s %s\n", "'$spec'", preg_replace('/^.*at position/', 'pos', $e->getMessage()));
    }
}

echo "--- the procedural doors report it the same way\n";
var_dump(strtotime('2020-01-01 january'));
$d = date_create('2020-06-15 12:30:45');
var_dump(@date_modify($d, '2020-01-01 january'));
echo $d->format('Y-m-d H:i:s'), "\n";
?>
--EXPECT--
'2020-01-01 january'                           pos 11 (j): Double date specification
'january january'                              pos 8 (j): Double date specification
'2020-01-01 2020-01-02'                        pos 11 (2): Double date specification
'20240102 20240102'                            pos 9 (2): Double date specification
'12:00 20240102'                               2024-01-02 12:00:00
'2020-01-01 20240102'                          pos 11 (2): Double date specification
'first day of january january'                 pos 21 (j): Double date specification
'first day of january last day of february 2021' pos 33 (f): Double date specification
'15 january 2020 march'                        pos 16 (m): Double date specification
'01/02/2020 03.04.2021'                        pos 11 (0): Double date specification
'2020-01-01 2020'                              2020-01-01 20:20:00
'1234 5678'                                    5678-06-15 12:34:00
'2020 Jan'                                     2020-01-01 00:00:00
'2020-01-01 12:00'                             2020-01-01 12:00:00
'2020-01-01 1234'                              2020-01-01 12:34:00
'january +1 day'                               2020-01-16 00:00:00
'first day of january +1 month'                2020-02-01 00:00:00
--- the procedural doors report it the same way
bool(false)
bool(false)
2020-06-15 12:30:45
