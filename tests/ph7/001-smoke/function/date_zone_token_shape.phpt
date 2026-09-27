--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's timezone token is matched by SHAPE, and looked up only when it is the first
--FILE--
<?php
/* php's scanner matches its TIMEZONE rule on shape and asks what the letters
 * spell only afterwards -- and only for the string's FIRST zone token, because
 * TIMELIB_HAVE_TZ runs before the lookup. So a second zone-shaped word is
 * dropped whatever it says and a third is `Double timezone specification`,
 * neither of them ever reported as a name the database does not have. PHL had
 * no shape rule at all: an unknown word simply ran out of rules, so
 * `GMT+3,11:30am.jan` -- a string php reads whole -- refused at the `am`.
 *
 * The rule is two alternatives, the longer winning:
 *   "("? [A-Za-z]{1,6} ")"?        each paren optional on its own
 *   [A-Z][a-z]+([_/-][A-Za-z]+)+   the tz-database identifier
 *
 * Its SIX-letter cap is what every other word-shaped rule competes against:
 * `janx` is an unknown zone, `januaryx` is January beside the military zone X,
 * and `augustx` is the six-letter tie the month wins. A unit word never
 * competes at all -- the number in front of it started the match -- so
 * `+1 dayx` is a day and the zone X. */
date_default_timezone_set('UTC');
$rows = ['jan', 'janx', 'january', 'januaryx', 'februaryx', 'augustx', 'marchx',
         'now', 'nowx', 'todayx', 'agox', 'noonx', 'midnightx', 'tomorrowx',
         'yesterdayx', 'weekdayx', 'weekdaysx', 'sundayx', 'mondayx', 'secondx',
         'previousx month', '+1 dayx', '+1 dayxyz', '+1 monthsx', '2 dayss',
         'UTC', 'UTCX', 'UTCXYZABC', '(UTC)', '(abc', 'abc)', 'GMT+02:00',
         '(GMT+02:00)', 'GMTx', 'Ab/cd', 'A/b', 'Ab.cd', 'Ab/cd-ef_gh',
         'Z,tues UTC', 'Z,tues,UTC,GMT', 'GMT+3,11:30am.jan', 'xyz abc'];
$base = '2020-06-15 08:09:10';
foreach ($rows as $s) {
    try { $d = new DateTime($base); $d->modify($s); $r = $d->format('Y-m-d H:i:s P'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-20s %s\n", $s, $r);
}
?>
--EXPECT--
jan                  2020-01-15 08:09:10 +00:00
janx                 DateTime::modify(): Failed to parse time string (janx) at position 0 (j): The timezone could not be found in the database
january              2020-01-15 08:09:10 +00:00
januaryx             2020-01-15 08:09:10 +00:00
februaryx            2020-02-15 08:09:10 +00:00
augustx              2020-08-15 08:09:10 +00:00
marchx               DateTime::modify(): Failed to parse time string (marchx) at position 0 (m): The timezone could not be found in the database
now                  2020-06-15 08:09:10 +00:00
nowx                 DateTime::modify(): Failed to parse time string (nowx) at position 0 (n): The timezone could not be found in the database
todayx               DateTime::modify(): Failed to parse time string (todayx) at position 0 (t): The timezone could not be found in the database
agox                 DateTime::modify(): Failed to parse time string (agox) at position 0 (a): The timezone could not be found in the database
noonx                DateTime::modify(): Failed to parse time string (noonx) at position 0 (n): The timezone could not be found in the database
midnightx            2020-06-15 00:00:00 +00:00
tomorrowx            2020-06-16 00:00:00 +00:00
yesterdayx           2020-06-14 00:00:00 +00:00
weekdayx             2020-06-15 00:00:00 +00:00
weekdaysx            2020-06-15 00:00:00 +00:00
sundayx              2020-06-21 00:00:00 +00:00
mondayx              2020-06-15 00:00:00 +00:00
secondx              DateTime::modify(): Failed to parse time string (secondx) at position 0 (s): The timezone could not be found in the database
previousx month      DateTime::modify(): Failed to parse time string (previousx month) at position 0 (p): The timezone could not be found in the database
+1 dayx              2020-06-16 08:09:10 +00:00
+1 dayxyz            DateTime::modify(): Failed to parse time string (+1 dayxyz) at position 6 (x): The timezone could not be found in the database
+1 monthsx           2020-07-15 08:09:10 +00:00
2 dayss              2020-06-17 08:09:10 +00:00
UTC                  2020-06-15 08:09:10 +00:00
UTCX                 DateTime::modify(): Failed to parse time string (UTCX) at position 0 (U): The timezone could not be found in the database
UTCXYZABC            DateTime::modify(): Failed to parse time string (UTCXYZABC) at position 0 (U): The timezone could not be found in the database
(UTC)                2020-06-15 08:09:10 +00:00
(abc                 DateTime::modify(): Failed to parse time string ((abc) at position 0 ((): The timezone could not be found in the database
abc)                 DateTime::modify(): Failed to parse time string (abc)) at position 0 (a): The timezone could not be found in the database
GMT+02:00            2020-06-15 08:09:10 +00:00
(GMT+02:00)          DateTime::modify(): Failed to parse time string ((GMT+02:00)) at position 10 ()): Unexpected character
GMTx                 DateTime::modify(): Failed to parse time string (GMTx) at position 0 (G): The timezone could not be found in the database
Ab/cd                DateTime::modify(): Failed to parse time string (Ab/cd) at position 0 (A): The timezone could not be found in the database
A/b                  DateTime::modify(): Failed to parse time string (A/b) at position 1 (/): Unexpected character
Ab.cd                DateTime::modify(): Failed to parse time string (Ab.cd) at position 0 (A): The timezone could not be found in the database
Ab/cd-ef_gh          DateTime::modify(): Failed to parse time string (Ab/cd-ef_gh) at position 0 (A): The timezone could not be found in the database
Z,tues UTC           DateTime::modify(): Failed to parse time string (Z,tues UTC) at position 7 (U): Double timezone specification
Z,tues,UTC,GMT       DateTime::modify(): Failed to parse time string (Z,tues,UTC,GMT) at position 7 (U): Double timezone specification
GMT+3,11:30am.jan    2020-01-15 11:30:00 +00:00
xyz abc              DateTime::modify(): Failed to parse time string (xyz abc) at position 0 (x): The timezone could not be found in the database
