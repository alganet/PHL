--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's @epoch is a token anywhere in a date string, and its action is ordered
--FILE--
<?php
/* php's scanner reads `@epoch` wherever it stands, not only at the head of the
 * string: `2020-01-02 @100` and `12:00 @100` are the epoch there, and were
 * refusals here. The shape is `"@" "-"? [0-9]+ ("." [0-9]{0,6})?` -- a PLUS is
 * no part of it, so `@+100` is an unexpected `@` with a UTC offset behind it,
 * and the fraction stops at six digits, which leaves `@100.1234567` refusing on
 * the seventh while `@100.1234567890` reads the trailing four as a year.
 *
 * The ACTION is ordered, and the order shows: TIMELIB_UNHAVE_DATE and
 * TIMELIB_UNHAVE_TIME first -- which ZERO the civil fields rather than
 * unsetting them, so an epoch behind a date reads back as the year 0 -- then
 * TIMELIB_HAVE_TZ, which RETURNS when the string already named a zone, so
 * `UTC @100` leaves nothing but those zeroes: neither 1970 nor the seconds are
 * ever written. An empty fraction is php's `Found unexpected data`, raised at
 * the `@` and AFTER the value, which is why `@100.,UTC` still reads 1970 plus a
 * hundred seconds -- when it is reached at all. */
date_default_timezone_set('UTC');
$rows = ['@100', '@+100', '@ 100', '@-100', '@-1.5', '@abc', '@100.5',
         '@100.123456', '@100.1234567', '@100.1234567890', '@100.', '@100.x',
         '@100.,UTC', 'UTC @100.', '@100@200', '@100 @200', '@100 UTC',
         '2020-01-02 @100', '12:00 @100', 'x @100', 'UTC @100', '@0 +1 day',
         '@100. @200', '@0 tomorrow'];
foreach ($rows as $s) {
    try { $r = (new DateTime($s))->format('Y-m-d H:i:s.u P'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-18s %s\n", $s, $r);
}
?>
--EXPECT--
@100               1970-01-01 00:01:40.000000 +00:00
@+100              Failed to parse time string (@+100) at position 0 (@): Unexpected character
@ 100              Failed to parse time string (@ 100) at position 0 (@): Unexpected character
@-100              1969-12-31 23:58:20.000000 +00:00
@-1.5              1969-12-31 23:59:58.500000 +00:00
@abc               Failed to parse time string (@abc) at position 0 (@): Unexpected character
@100.5             1970-01-01 00:01:40.500000 +00:00
@100.123456        1970-01-01 00:01:40.123456 +00:00
@100.1234567       Failed to parse time string (@100.1234567) at position 11 (7): Unexpected character
@100.1234567890    7890-01-01 00:01:40.123456 +00:00
@100.              Failed to parse time string (@100.) at position 0 (@): Found unexpected data
@100.x             Failed to parse time string (@100.x) at position 0 (@): Found unexpected data
@100.,UTC          Failed to parse time string (@100.,UTC) at position 0 (@): Found unexpected data
UTC @100.          -0001-11-30 00:00:00.000000 +00:00
@100@200           -0001-11-30 00:01:40.000000 +00:00
@100 @200          -0001-11-30 00:01:40.000000 +00:00
@100 UTC           1970-01-01 00:01:40.000000 +00:00
2020-01-02 @100    1970-01-01 00:01:40.000000 +00:00
12:00 @100         1970-01-01 00:01:40.000000 +00:00
x @100             -0001-11-30 00:00:00.000000 -11:00
UTC @100           -0001-11-30 00:00:00.000000 +00:00
@0 +1 day          1970-01-02 00:00:00.000000 +00:00
@100. @200         Failed to parse time string (@100. @200) at position 0 (@): Found unexpected data
@0 tomorrow        1970-01-02 00:00:00.000000 +00:00
