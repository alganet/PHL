--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A relative date string is gathered whole, then applied in php's order
--FILE--
<?php
/* php's date parse is a FIELD parse: timelib fills a civil vector plus a
 * separate relative one and applies NOTHING until the whole string has been
 * read. PHL applied each unit to the clock as it scanned, so the WRITTEN order
 * decided the answer -- `+30 days +1 month` was a day off there and php reads
 * the two orders as one thing.
 *
 * Under that are the rules the order model cannot express: the weekday hunt
 * runs BEFORE the relative vector, `first|last day of` is applied twice (once
 * before it, once after, which is why it swallows the days beside it and not
 * the hours), `tomorrow`/`yesterday` SET the relative day rather than adding to
 * it, and a `... week` navigation is a move to that week's Monday. */
date_default_timezone_set('UTC');

$base = '2020-06-15 00:00:00';   /* a Monday; the time of day a bare date keeps
                                  * is its own family, and its own test */

echo "--- the written order does not decide\n";
foreach ([['+30 days +1 month', '+1 month +30 days'],
          ['+1 day +1 month', '+1 month +1 day'],
          ['+2 weeks +1 month', '+1 month +2 weeks'],
          ['+1 month +100 hours', '+100 hours +1 month'],
          ['+30 days next monday', 'next monday +30 days'],
          ['+27 seconds next monday', 'next monday +27 seconds'],
          ['-2030 week 1072 month', '1072 month -2030 week']] as $pair) {
    $out = [];
    foreach ($pair as $spec) {
        $d = new DateTime($base);
        $d->modify($spec);
        $out[] = $d->format('Y-m-d H:i:s');
    }
    printf("%-24s %s | %-24s %s%s\n", "'$pair[0]'", $out[0], "'$pair[1]'", $out[1],
        $out[0] === $out[1] ? '' : '  <-- DIFFER');
}

echo "--- months land on the day before the days move it\n";
foreach (['2020-01-31 +1 month', '2020-01-31 +1 month +1 day', '2007-02-31 -3 months',
          '2020-01-31 +30 days +1 month', '2020-06-30 +1 month'] as $spec) {
    printf("%-30s %s\n", "'$spec'", date('Y-m-d', strtotime($spec, strtotime($base))));
}

echo "--- first|last day of is applied twice\n";
foreach (['first day of next month', 'first day of next month +40 days',
          '+40 days first day of next month', 'last day of this month +5 days',
          '+3 hours first day of next month', 'first day of january',
          'last day of february 2021', 'last monday first day of this month',
          '1010-09-31 first day of next month'] as $spec) {
    $d = new DateTime($base);
    $d->modify($spec);
    printf("%-38s %s\n", "'$spec'", $d->format('Y-m-d H:i:s'));
}

echo "--- the weekday hunt runs first, and the week words move to a Monday\n";
foreach (['2020-06-15', '2020-06-17', '2020-06-21'] as $day) {
    foreach (['monday', 'this monday', 'next monday', 'last monday', 'sunday',
              'next sunday', 'this week', 'next week', 'last week',
              'monday this week', 'sunday this week', 'monday next week'] as $spec) {
        $d = new DateTime($day . ' 00:00:00');
        $d->modify($spec);
        printf("%s %-18s %s\n", $day, "'$spec'", $d->format('Y-m-d H:i'));
    }
}

echo "--- tomorrow and yesterday SET the relative day\n";
foreach (['tomorrow', 'yesterday', 'tomorrow yesterday', 'yesterday tomorrow',
          '+3 days tomorrow', 'tomorrow +3 days', '+5 weeks tomorrow',
          'last week yesterday', 'yesterday last week', 'today', 'noon',
          '2020-06-30 tomorrow +1 month'] as $spec) {
    $d = new DateTime($base);
    $d->modify($spec);
    printf("%-28s %s\n", "'$spec'", $d->format('Y-m-d H:i:s'));
}

echo "--- a month name is absolute wherever it stands\n";
foreach (['january', 'january 2020', 'march 3', '15 january', '+1 day january',
          'january +1 day'] as $spec) {
    printf("%-18s %s\n", "'$spec'", date('Y-m-d H:i:s', strtotime($spec, strtotime($base))));
}

echo "--- a trailing UTC/GMT is the string's own zone, and beats the argument\n";
foreach (['1 january 2020 UTC', '1 january 2020 utc', '1 january 2020 GMT',
          'jan 1 2020 12:00 gmt', '2020-01-01T00:00:00Z'] as $spec) {
    foreach ([null, '+05:00'] as $tz) {
        $d = $tz === null ? new DateTime($spec) : new DateTime($spec, new DateTimeZone($tz));
        $z = $d->getTimezone();
        $a = (array) $z;
        printf("%-22s arg=%-7s %s | %s type %d\n", "'$spec'", $tz ?? '-',
            $d->format('Y-m-d H:i:s P'), $z->getName(), $a['timezone_type']);
    }
}

echo "--- the epoch is 1970 plus a relative second count\n";
foreach (['@0', '@0 +1 day', '@0 12:00', '@86400 -1 hour', '@1600000000.5',
          '@-777178018.5', '@-1021353077.000001', '@-1.5'] as $spec) {
    $d = new DateTime($spec);
    printf("%-22s %s | strtotime %s\n", "'$spec'", $d->format('Y-m-d H:i:s.u'),
        var_export(strtotime($spec), true));
}
?>
--EXPECT--
--- the written order does not decide
'+30 days +1 month'      2020-08-14 00:00:00 | '+1 month +30 days'      2020-08-14 00:00:00
'+1 day +1 month'        2020-07-16 00:00:00 | '+1 month +1 day'        2020-07-16 00:00:00
'+2 weeks +1 month'      2020-07-29 00:00:00 | '+1 month +2 weeks'      2020-07-29 00:00:00
'+1 month +100 hours'    2020-07-19 04:00:00 | '+100 hours +1 month'    2020-07-19 04:00:00
'+30 days next monday'   2020-07-22 00:00:00 | 'next monday +30 days'   2020-07-22 00:00:00
'+27 seconds next monday' 2020-06-22 00:00:27 | 'next monday +27 seconds' 2020-06-22 00:00:27
'-2030 week 1072 month'  2070-11-18 00:00:00 | '1072 month -2030 week'  2070-11-18 00:00:00
--- months land on the day before the days move it
'2020-01-31 +1 month'          2020-03-02
'2020-01-31 +1 month +1 day'   2020-03-03
'2007-02-31 -3 months'         2006-12-03
'2020-01-31 +30 days +1 month' 2020-04-01
'2020-06-30 +1 month'          2020-07-30
--- first|last day of is applied twice
'first day of next month'              2020-07-01 00:00:00
'first day of next month +40 days'     2020-07-01 00:00:00
'+40 days first day of next month'     2020-07-01 00:00:00
'last day of this month +5 days'       2020-06-30 00:00:00
'+3 hours first day of next month'     2020-07-01 03:00:00
'first day of january'                 2020-01-01 00:00:00
'last day of february 2021'            2021-02-28 00:00:00
'last monday first day of this month'  2020-06-01 00:00:00
'1010-09-31 first day of next month'   1010-10-01 00:00:00
--- the weekday hunt runs first, and the week words move to a Monday
2020-06-15 'monday'           2020-06-15 00:00
2020-06-15 'this monday'      2020-06-15 00:00
2020-06-15 'next monday'      2020-06-22 00:00
2020-06-15 'last monday'      2020-06-08 00:00
2020-06-15 'sunday'           2020-06-21 00:00
2020-06-15 'next sunday'      2020-06-21 00:00
2020-06-15 'this week'        2020-06-15 00:00
2020-06-15 'next week'        2020-06-22 00:00
2020-06-15 'last week'        2020-06-08 00:00
2020-06-15 'monday this week' 2020-06-15 00:00
2020-06-15 'sunday this week' 2020-06-21 00:00
2020-06-15 'monday next week' 2020-06-22 00:00
2020-06-17 'monday'           2020-06-22 00:00
2020-06-17 'this monday'      2020-06-22 00:00
2020-06-17 'next monday'      2020-06-22 00:00
2020-06-17 'last monday'      2020-06-15 00:00
2020-06-17 'sunday'           2020-06-21 00:00
2020-06-17 'next sunday'      2020-06-21 00:00
2020-06-17 'this week'        2020-06-15 00:00
2020-06-17 'next week'        2020-06-22 00:00
2020-06-17 'last week'        2020-06-08 00:00
2020-06-17 'monday this week' 2020-06-15 00:00
2020-06-17 'sunday this week' 2020-06-21 00:00
2020-06-17 'monday next week' 2020-06-22 00:00
2020-06-21 'monday'           2020-06-22 00:00
2020-06-21 'this monday'      2020-06-22 00:00
2020-06-21 'next monday'      2020-06-22 00:00
2020-06-21 'last monday'      2020-06-15 00:00
2020-06-21 'sunday'           2020-06-21 00:00
2020-06-21 'next sunday'      2020-06-28 00:00
2020-06-21 'this week'        2020-06-15 00:00
2020-06-21 'next week'        2020-06-22 00:00
2020-06-21 'last week'        2020-06-08 00:00
2020-06-21 'monday this week' 2020-06-15 00:00
2020-06-21 'sunday this week' 2020-06-21 00:00
2020-06-21 'monday next week' 2020-06-22 00:00
--- tomorrow and yesterday SET the relative day
'tomorrow'                   2020-06-16 00:00:00
'yesterday'                  2020-06-14 00:00:00
'tomorrow yesterday'         2020-06-14 00:00:00
'yesterday tomorrow'         2020-06-16 00:00:00
'+3 days tomorrow'           2020-06-16 00:00:00
'tomorrow +3 days'           2020-06-19 00:00:00
'+5 weeks tomorrow'          2020-06-16 00:00:00
'last week yesterday'        2020-06-14 00:00:00
'yesterday last week'        2020-06-07 00:00:00
'today'                      2020-06-15 00:00:00
'noon'                       2020-06-15 12:00:00
'2020-06-30 tomorrow +1 month' 2020-07-31 00:00:00
--- a month name is absolute wherever it stands
'january'          2020-01-15 00:00:00
'january 2020'     2020-01-01 00:00:00
'march 3'          2020-03-03 00:00:00
'15 january'       2020-01-15 00:00:00
'+1 day january'   2020-01-16 00:00:00
'january +1 day'   2020-01-16 00:00:00
--- a trailing UTC/GMT is the string's own zone, and beats the argument
'1 january 2020 UTC'   arg=-       2020-01-01 00:00:00 +00:00 | UTC type 3
'1 january 2020 UTC'   arg=+05:00  2020-01-01 00:00:00 +00:00 | UTC type 3
'1 january 2020 utc'   arg=-       2020-01-01 00:00:00 +00:00 | UTC type 2
'1 january 2020 utc'   arg=+05:00  2020-01-01 00:00:00 +00:00 | UTC type 2
'1 january 2020 GMT'   arg=-       2020-01-01 00:00:00 +00:00 | GMT type 2
'1 january 2020 GMT'   arg=+05:00  2020-01-01 00:00:00 +00:00 | GMT type 2
'jan 1 2020 12:00 gmt' arg=-       2020-01-01 12:00:00 +00:00 | GMT type 2
'jan 1 2020 12:00 gmt' arg=+05:00  2020-01-01 12:00:00 +00:00 | GMT type 2
'2020-01-01T00:00:00Z' arg=-       2020-01-01 00:00:00 +00:00 | Z type 2
'2020-01-01T00:00:00Z' arg=+05:00  2020-01-01 00:00:00 +00:00 | Z type 2
--- the epoch is 1970 plus a relative second count
'@0'                   1970-01-01 00:00:00.000000 | strtotime 0
'@0 +1 day'            1970-01-02 00:00:00.000000 | strtotime 86400
'@0 12:00'             1970-01-01 12:00:00.000000 | strtotime 43200
'@86400 -1 hour'       1970-01-01 23:00:00.000000 | strtotime 82800
'@1600000000.5'        2020-09-13 12:26:40.500000 | strtotime 1600000000
'@-777178018.5'        1945-05-16 21:13:01.500000 | strtotime -777178019
'@-1021353077.000001'  1937-08-20 18:48:42.999999 | strtotime -1021353078
'@-1.5'                1969-12-31 23:59:58.500000 | strtotime -2
