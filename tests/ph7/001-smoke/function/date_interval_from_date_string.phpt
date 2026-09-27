--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
createFromDateString refuses an absolute element and reads the words
--FILE--
<?php
/* php builds this interval out of timelib's RELATIVE vector and then checks that
 * the string set nothing absolute -- no date, no time of day, no zone -- so a
 * mistyped duration is `String '12:34' contains non-relative elements` there.
 * PHL scanned the text a second time for `<number> <unit>` pairs, which answered
 * an all-zero interval instead: a duration that moves nothing.
 *
 * The same vector is why php's navigation WORDS carry fields: `tomorrow` is
 * d = 1, `first day of next month` is m = 1, `+1 week 2 days` is d = 9, and the
 * words php really does ignore (a weekday that only names a day) stay zero. */
date_default_timezone_set('UTC');

echo "--- what php refuses\n";
foreach (['12:34', '2020-01-01', 'noon', '@0', '1 day 12:00', 'midnight',
          '2020-01-01 12:00', '1234', '20240102', 'last day of february 2021',
          '15 january 2020'] as $spec) {
    try {
        DateInterval::createFromDateString($spec);
        printf("%-28s built\n", "'$spec'");
    } catch (Throwable $e) {
        printf("%-28s %s: %s\n", "'$spec'", get_class($e), $e->getMessage());
    }
}

echo "--- what it reads\n";
foreach (['tomorrow', 'yesterday', 'today', 'first day of next month',
          'first day of last month', '+1 week 2 days', '-1 week', 'next week',
          'last week', 'this week', 'saturday this week', 'next monday',
          '+1 month', '-2 years 3 days', '1 day 2 hours 3 mins 4 secs',
          '500 microseconds', '1000000 microseconds', '-1 hour -1 ms',
          '+1 fortnight', 'next month'] as $spec) {
    $iv = DateInterval::createFromDateString($spec);
    printf("%-30s y=%d m=%d d=%d h=%d i=%d s=%d f=%s\n", "'$spec'",
        $iv->y, $iv->m, $iv->d, $iv->h, $iv->i, $iv->s, var_export($iv->f, true));
}

echo "--- the procedural door warns instead\n";
var_dump(@date_interval_create_from_date_string('12:34'));
$iv = date_interval_create_from_date_string('tomorrow');
printf("d=%d\n", $iv->d);

echo "--- and an interval that reads nothing is still an interval\n";
$iv = DateInterval::createFromDateString('saturday this week');
printf("%s | %s\n", $iv->format('%R%y-%m-%d %h:%i:%s'), var_export($iv->days, true));
?>
--EXPECT--
--- what php refuses
'12:34'                      DateMalformedIntervalStringException: String '12:34' contains non-relative elements
'2020-01-01'                 DateMalformedIntervalStringException: String '2020-01-01' contains non-relative elements
'noon'                       DateMalformedIntervalStringException: String 'noon' contains non-relative elements
'@0'                         DateMalformedIntervalStringException: String '@0' contains non-relative elements
'1 day 12:00'                DateMalformedIntervalStringException: String '1 day 12:00' contains non-relative elements
'midnight'                   built
'2020-01-01 12:00'           DateMalformedIntervalStringException: String '2020-01-01 12:00' contains non-relative elements
'1234'                       DateMalformedIntervalStringException: String '1234' contains non-relative elements
'20240102'                   DateMalformedIntervalStringException: String '20240102' contains non-relative elements
'last day of february 2021'  DateMalformedIntervalStringException: String 'last day of february 2021' contains non-relative elements
'15 january 2020'            DateMalformedIntervalStringException: String '15 january 2020' contains non-relative elements
--- what it reads
'tomorrow'                     y=0 m=0 d=1 h=0 i=0 s=0 f=0.0
'yesterday'                    y=0 m=0 d=-1 h=0 i=0 s=0 f=0.0
'today'                        y=0 m=0 d=0 h=0 i=0 s=0 f=0.0
'first day of next month'      y=0 m=1 d=0 h=0 i=0 s=0 f=0.0
'first day of last month'      y=0 m=-1 d=0 h=0 i=0 s=0 f=0.0
'+1 week 2 days'               y=0 m=0 d=9 h=0 i=0 s=0 f=0.0
'-1 week'                      y=0 m=0 d=-7 h=0 i=0 s=0 f=0.0
'next week'                    y=0 m=0 d=7 h=0 i=0 s=0 f=0.0
'last week'                    y=0 m=0 d=-7 h=0 i=0 s=0 f=0.0
'this week'                    y=0 m=0 d=0 h=0 i=0 s=0 f=0.0
'saturday this week'           y=0 m=0 d=0 h=0 i=0 s=0 f=0.0
'next monday'                  y=0 m=0 d=0 h=0 i=0 s=0 f=0.0
'+1 month'                     y=0 m=1 d=0 h=0 i=0 s=0 f=0.0
'-2 years 3 days'              y=-2 m=0 d=3 h=0 i=0 s=0 f=0.0
'1 day 2 hours 3 mins 4 secs'  y=0 m=0 d=1 h=2 i=3 s=4 f=0.0
'500 microseconds'             y=0 m=0 d=0 h=0 i=0 s=0 f=0.0005
'1000000 microseconds'         y=0 m=0 d=0 h=0 i=0 s=0 f=1.0
'-1 hour -1 ms'                y=0 m=0 d=0 h=-1 i=0 s=0 f=-0.001
'+1 fortnight'                 y=0 m=0 d=14 h=0 i=0 s=0 f=0.0
'next month'                   y=0 m=1 d=0 h=0 i=0 s=0 f=0.0
--- the procedural door warns instead
bool(false)
d=1
--- and an interval that reads nothing is still an interval
+0-0-0 0:0:0 | false
