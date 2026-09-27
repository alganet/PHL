--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php reads a timezone as a TOKEN, wherever the string spells one
--FILE--
<?php
/* php's scanner takes a timezone anywhere in a date string, as a token of its
 * own: a NAME, a name in parentheses, or a UTC offset with an optional uppercase
 * `GMT` in front of it. PHL read one only where it was ATTACHED -- after the time
 * of an ISO date, or trailing a textual-month date -- so `2020-01-01 12:00 UTC`,
 * php's own serialization spelling `<date> <timezone>`, and every RFC-2822 date
 * there is (`Thu, 01 Jan 2020 12:00:00 +0000`) did not parse at all.
 *
 * The FIRST token wins outright and the second is ignored in silence; a THIRD is
 * php's "Double timezone specification", which is what makes `-123-03-04` -- three
 * offsets to php's scanner, and no date at all -- a refusal at its last `-`.
 *
 * The offset's own grammar is php's longest match, and it disagrees with the
 * VALUE php then computes from the byte count: `+099` matches as the hour `09`
 * and the minute `9`, then counts as three digits and answers 0h99m = +01:39. */
date_default_timezone_set('UTC');

function dtz_show($spec, $fmt = 'Y-m-d H:i:s P')
{
    try {
        $d = new DateTime($spec);
        $a = (array)$d;
        printf("%-32s %s tt=%d tz=%s\n", $spec, $d->format($fmt),
            $a['timezone_type'], $a['timezone']);
    } catch (Throwable $e) {
        printf("%-32s %s\n", $spec, $e->getMessage());
    }
}

echo "--- a NAME, wherever it stands\n";
foreach (['2020-01-01 12:00 UTC', '2020-01-01 12:00 GMT', '2020-01-01T12:00:00 UTC',
          '2020-01-01 UTC', '2020-01-01UTC', '2020-01-01 12:00:00UTC', '2020-01-01 12:00 utc',
          '2020-01-01 12:00 Z', '2020-01-01 12:00 z',
          'UTC 2020-01-01 12:00', '2020-01-01 UTC 12:00', '2020-01-01 12:00 (GMT)',
          '2020-01-01(UTC)'] as $spec) {
    dtz_show($spec);
}

echo "--- an OFFSET, wherever it stands\n";
foreach (['2020-01-01 12:00 +0200', '2020-01-01 12:00:00 -0500', '2020-01-01 12:00 +02',
          '2020-01-01 12:00 +2', '2020-01-01 12:00 GMT+02:00', '2020-01-01 12:00 gmt',
          '+0200 2020-01-01', '2020-01-01 -0500 12:00',
          'Thu, 01 Jan 2020 12:00:00 +0000', 'Thu, 01 Jan 2020 12:00:00 GMT',
          '2020-01-01 12:00 +020030', '2020-01-01T12:00:00+020030',
          '2020-01-01 12:00:00.5 +0200'] as $spec) {
    dtz_show($spec);
}
foreach (['12:00 +0200', '12:00+0200'] as $spec) {     /* no date: the clock alone */
    dtz_show($spec, 'H:i:s P');
}

echo "--- php's offset SHAPES: the longest match, then the value by width\n";
foreach (['2020-01-01 +123', '2020-01-01 +1234', '2020-01-01 +099', '2020-01-01 +060',
          '2020-01-01 +960', '2020-01-01 +999', '2020-01-01 +9999', '2020-01-01 +2460',
          '2020-01-01 +2559', '2020-01-01 +245959', '2020-01-01 +255959', '2020-01-01 +000060',
          '2020-01-01 +000061', '2020-01-01 +1:2', '2020-01-01 +01:2', '2020-01-01 +9:59',
          '2020-01-01 +25:00', '2020-01-01 +24:59:59', '2020-01-01 +1:00:59',
          '2020-01-01 +00:00:60', '2020-01-01 +02003', '2020-01-01 +02:'] as $spec) {
    dtz_show($spec);
}

echo "--- the first wins, the second is ignored, the third refused\n";
foreach (['2020-01-01 +0200 +0300', '2020-01-01 UTC GMT', '2020-01-01 UTC +0200',
          '2020-01-01 +0200 UTC', '2020-01-01T12:00:00+02:00 UTC', '2020-01-01T12:00:00Z +0300',
          '2020-01-01 +02-03', '2020-01-01 -123-03', '2020-01-01 12:00 -03 -04',
          'UTC GMT Z', '+02 UTC GMT', 'UTC +02 +03', 'Z Z Z', '-123-03-04', '-12-03-04',
          '-1-03-04', '+02-03-04', '+1 -2 -3'] as $spec) {
    dtz_show($spec);
}

echo "--- a name php cannot find, and a byte it did not expect\n";
foreach (['UTC1', 'UTCX', 'ZZ', 'ut', '2020-01-01 12:00 UT', '2020-01-01 12:00 xyz',
          '(foo)', '( UTC )', '2020-01-01 12:00 GMT+', 'america/new_york'] as $spec) {
    dtz_show($spec);
}

echo "--- the token names the zone the CIVIL fields are read in\n";
var_dump(strtotime('2020-01-01 12:00:00 +0200'));
var_dump(strtotime('2020-01-01 12:00:00 UTC'));
var_dump(strtotime('2020-01-01 12:00:00 -0500'));
echo date('Y-m-d H:i:s', strtotime('Thu, 01 Jan 2020 12:00:00 +0000')), "\n";
?>
--EXPECT--
--- a NAME, wherever it stands
2020-01-01 12:00 UTC             2020-01-01 12:00:00 +00:00 tt=3 tz=UTC
2020-01-01 12:00 GMT             2020-01-01 12:00:00 +00:00 tt=2 tz=GMT
2020-01-01T12:00:00 UTC          2020-01-01 12:00:00 +00:00 tt=3 tz=UTC
2020-01-01 UTC                   2020-01-01 00:00:00 +00:00 tt=3 tz=UTC
2020-01-01UTC                    2020-01-01 00:00:00 +00:00 tt=3 tz=UTC
2020-01-01 12:00:00UTC           2020-01-01 12:00:00 +00:00 tt=3 tz=UTC
2020-01-01 12:00 utc             2020-01-01 12:00:00 +00:00 tt=2 tz=UTC
2020-01-01 12:00 Z               2020-01-01 12:00:00 +00:00 tt=2 tz=Z
2020-01-01 12:00 z               2020-01-01 12:00:00 +00:00 tt=2 tz=Z
UTC 2020-01-01 12:00             2020-01-01 12:00:00 +00:00 tt=3 tz=UTC
2020-01-01 UTC 12:00             2020-01-01 12:00:00 +00:00 tt=3 tz=UTC
2020-01-01 12:00 (GMT)           2020-01-01 12:00:00 +00:00 tt=2 tz=GMT
2020-01-01(UTC)                  2020-01-01 00:00:00 +00:00 tt=3 tz=UTC
--- an OFFSET, wherever it stands
2020-01-01 12:00 +0200           2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00
2020-01-01 12:00:00 -0500        2020-01-01 12:00:00 -05:00 tt=1 tz=-05:00
2020-01-01 12:00 +02             2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00
2020-01-01 12:00 +2              2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00
2020-01-01 12:00 GMT+02:00       2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00
2020-01-01 12:00 gmt             2020-01-01 12:00:00 +00:00 tt=2 tz=GMT
+0200 2020-01-01                 2020-01-01 00:00:00 +02:00 tt=1 tz=+02:00
2020-01-01 -0500 12:00           2020-01-01 12:00:00 -05:00 tt=1 tz=-05:00
Thu, 01 Jan 2020 12:00:00 +0000  2020-01-02 12:00:00 +00:00 tt=1 tz=+00:00
Thu, 01 Jan 2020 12:00:00 GMT    2020-01-02 12:00:00 +00:00 tt=2 tz=GMT
2020-01-01 12:00 +020030         2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00:30
2020-01-01T12:00:00+020030       2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00:30
2020-01-01 12:00:00.5 +0200      2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00
12:00 +0200                      12:00:00 +02:00 tt=1 tz=+02:00
12:00+0200                       12:00:00 +02:00 tt=1 tz=+02:00
--- php's offset SHAPES: the longest match, then the value by width
2020-01-01 +123                  2020-01-01 00:00:00 +01:23 tt=1 tz=+01:23
2020-01-01 +1234                 2020-01-01 00:00:00 +12:34 tt=1 tz=+12:34
2020-01-01 +099                  2020-01-01 00:00:00 +01:39 tt=1 tz=+01:39
2020-01-01 +060                  2020-01-01 00:00:00 +01:00 tt=1 tz=+01:00
2020-01-01 +960                  Failed to parse time string (2020-01-01 +960) at position 14 (0): Unexpected character
2020-01-01 +999                  Failed to parse time string (2020-01-01 +999) at position 14 (9): Unexpected character
2020-01-01 +9999                 Failed to parse time string (2020-01-01 +9999) at position 14 (9): Unexpected character
2020-01-01 +2460                 Failed to parse time string (2020-01-01 +2460) at position 15 (0): Unexpected character
2020-01-01 +2559                 Failed to parse time string (2020-01-01 +2559) at position 15 (9): Unexpected character
2020-01-01 +245959               2020-01-01 00:00:00 +24:59 tt=1 tz=+24:59:59
2020-01-01 +255959               Failed to parse time string (2020-01-01 +255959) at position 15 (9): Unexpected character
2020-01-01 +000060               2020-01-01 00:00:00 +00:01 tt=1 tz=+00:01
2020-01-01 +000061               Failed to parse time string (2020-01-01 +000061) at position 16 (6): Unexpected character
2020-01-01 +1:2                  2020-01-01 00:00:00 +01:02 tt=1 tz=+01:02
2020-01-01 +01:2                 2020-01-01 00:00:00 +01:02 tt=1 tz=+01:02
2020-01-01 +9:59                 2020-01-01 00:00:00 +09:59 tt=1 tz=+09:59
2020-01-01 +25:00                Failed to parse time string (2020-01-01 +25:00) at position 14 (:): Unexpected character
2020-01-01 +24:59:59             2020-01-01 00:00:00 +24:59 tt=1 tz=+24:59:59
2020-01-01 +1:00:59              Failed to parse time string (2020-01-01 +1:00:59) at position 16 (:): Unexpected character
2020-01-01 +00:00:60             2020-01-01 00:00:00 +00:01 tt=1 tz=+00:01
2020-01-01 +02003                Failed to parse time string (2020-01-01 +02003) at position 16 (3): Unexpected character
2020-01-01 +02:                  Failed to parse time string (2020-01-01 +02:) at position 14 (:): Unexpected character
--- the first wins, the second is ignored, the third refused
2020-01-01 +0200 +0300           2020-01-01 00:00:00 +02:00 tt=1 tz=+02:00
2020-01-01 UTC GMT               2020-01-01 00:00:00 +00:00 tt=3 tz=UTC
2020-01-01 UTC +0200             2020-01-01 00:00:00 +00:00 tt=3 tz=UTC
2020-01-01 +0200 UTC             2020-01-01 00:00:00 +02:00 tt=1 tz=+02:00
2020-01-01T12:00:00+02:00 UTC    2020-01-01 12:00:00 +02:00 tt=1 tz=+02:00
2020-01-01T12:00:00Z +0300       2020-01-01 12:00:00 +00:00 tt=2 tz=Z
2020-01-01 +02-03                2020-01-01 00:00:00 +02:00 tt=1 tz=+02:00
2020-01-01 -123-03               2020-01-01 00:00:00 -01:23 tt=1 tz=-01:23
2020-01-01 12:00 -03 -04         2020-01-01 12:00:00 -03:00 tt=1 tz=-03:00
UTC GMT Z                        Failed to parse time string (UTC GMT Z) at position 8 (Z): Double timezone specification
+02 UTC GMT                      Failed to parse time string (+02 UTC GMT) at position 8 (G): Double timezone specification
UTC +02 +03                      Failed to parse time string (UTC +02 +03) at position 8 (+): Double timezone specification
Z Z Z                            Failed to parse time string (Z Z Z) at position 4 (Z): Double timezone specification
-123-03-04                       Failed to parse time string (-123-03-04) at position 7 (-): Double timezone specification
-12-03-04                        Failed to parse time string (-12-03-04) at position 6 (-): Double timezone specification
-1-03-04                         Failed to parse time string (-1-03-04) at position 5 (-): Double timezone specification
+02-03-04                        Failed to parse time string (+02-03-04) at position 6 (-): Double timezone specification
+1 -2 -3                         Failed to parse time string (+1 -2 -3) at position 6 (-): Double timezone specification
--- a name php cannot find, and a byte it did not expect
UTC1                             Failed to parse time string (UTC1) at position 3 (1): Unexpected character
UTCX                             Failed to parse time string (UTCX) at position 0 (U): The timezone could not be found in the database
ZZ                               Failed to parse time string (ZZ) at position 0 (Z): The timezone could not be found in the database
ut                               Failed to parse time string (ut) at position 0 (u): The timezone could not be found in the database
2020-01-01 12:00 UT              Failed to parse time string (2020-01-01 12:00 UT) at position 17 (U): The timezone could not be found in the database
2020-01-01 12:00 xyz             Failed to parse time string (2020-01-01 12:00 xyz) at position 17 (x): The timezone could not be found in the database
(foo)                            Failed to parse time string ((foo)) at position 0 ((): The timezone could not be found in the database
( UTC )                          Failed to parse time string (( UTC )) at position 0 ((): Unexpected character
2020-01-01 12:00 GMT+            Failed to parse time string (2020-01-01 12:00 GMT+) at position 20 (+): Unexpected character
america/new_york                 Failed to parse time string (america/new_york) at position 0 (a): The timezone could not be found in the database
--- the token names the zone the CIVIL fields are read in
int(1577872800)
int(1577880000)
int(1577898000)
2020-01-02 12:00:00
