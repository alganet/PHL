--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The relative words php reads that this engine refused
--FILE--
<?php
/* Four spellings php understands were parse failures here, and `2 days ago` is
 * as common as any of them: `ago`, which NEGATES the relative vector as it
 * stands (so a second one puts it back, and `next monday ago` is the Monday
 * before); `previous` as a synonym of `last`; every unit after one of those
 * words rather than the two the engine knew (`next hour`, `last year`,
 * `previous day` — php reads them as the ordinary unit with an amount of 0, 1 or
 * -1); and `weekday`, a count of BUSINESS days with an arithmetic of its own.
 *
 * That count is applied after everything else -- the relative vector and the
 * `first|last day of` flag both -- and php SETS it rather than adding, so only
 * the last one in a string moves anything. A bare `weekday` is not that count at
 * all there but the Monday hunt, the same answer a bare weekday NAME gives. */
date_default_timezone_set('UTC');

$base = '2020-06-15 12:30:45';   /* a Monday */

echo "--- ago\n";
foreach (['2 days ago', '2 weeks ago', '+1 month ago', '1 day 2 hours ago',
          'ago', '-2 days ago', '2 days ago ago', 'ago 2 days', 'next monday ago',
          '+19 msec ago'] as $spec) {
    $d = new DateTime($base);
    $d->modify($spec);
    printf("%-20s %s\n", "'$spec'", $d->format('Y-m-d H:i:s.u'));
}

echo "--- previous, and every unit after a word\n";
foreach (['previous week', 'previous month', 'previous day', 'previous year',
          'previous monday', 'next day', 'last day', 'this day', 'next hour',
          'last hour', 'next minute', 'next second', 'next fortnight',
          'this year', 'next year', 'last year', 'next years', 'last days'] as $spec) {
    $d = new DateTime($base);
    $d->modify($spec);
    printf("%-18s %s\n", "'$spec'", $d->format('Y-m-d H:i:s'));
}

echo "--- business days, from every day of the week\n";
for ($day = 14; $day <= 20; $day++) {
    $from = sprintf('2020-06-%02d 12:00:00', $day);
    $line = date('D', strtotime($from)) . ' ';
    foreach ([-8, -5, -2, -1, 0, 1, 2, 5, 8] as $count) {
        $d = new DateTime($from);
        $d->modify("$count weekdays");
        $line .= sprintf('%d:%s ', $count, $d->format('m-d'));
    }
    echo $line, "\n";
}

echo "--- and how it combines\n";
foreach (['weekday', 'weekdays', 'this weekday', 'next weekday', 'last weekday',
          '2 weekday', '+20 weekday -5 weekdays', 'last weekday +22 sec -8 months',
          'first day of next month +9 weekdays', 'last day of this month -11 weekdays',
          'yesterday +28 weekdays ago', 'last week weekday', 'this week weekday'] as $spec) {
    $d = new DateTime($base);
    $d->modify($spec);
    printf("%-38s %s\n", "'$spec'", $d->format('Y-m-d H:i:s'));
}
?>
--EXPECT--
--- ago
'2 days ago'         2020-06-13 12:30:45.000000
'2 weeks ago'        2020-06-01 12:30:45.000000
'+1 month ago'       2020-05-15 12:30:45.000000
'1 day 2 hours ago'  2020-06-14 10:30:45.000000
'ago'                2020-06-15 12:30:45.000000
'-2 days ago'        2020-06-17 12:30:45.000000
'2 days ago ago'     2020-06-17 12:30:45.000000
'ago 2 days'         2020-06-17 12:30:45.000000
'next monday ago'    2020-06-08 00:00:00.000000
'+19 msec ago'       2020-06-15 12:30:45.019000
--- previous, and every unit after a word
'previous week'    2020-06-08 12:30:45
'previous month'   2020-05-15 12:30:45
'previous day'     2020-06-14 12:30:45
'previous year'    2019-06-15 12:30:45
'previous monday'  2020-06-08 00:00:00
'next day'         2020-06-16 12:30:45
'last day'         2020-06-14 12:30:45
'this day'         2020-06-15 12:30:45
'next hour'        2020-06-15 13:30:45
'last hour'        2020-06-15 11:30:45
'next minute'      2020-06-15 12:31:45
'next second'      2020-06-15 12:30:46
'next fortnight'   2020-06-29 12:30:45
'this year'        2020-06-15 12:30:45
'next year'        2021-06-15 12:30:45
'last year'        2019-06-15 12:30:45
'next years'       2021-06-15 12:30:45
'last days'        2020-06-14 12:30:45
--- business days, from every day of the week
Sun -8:06-03 -5:06-08 -2:06-11 -1:06-12 0:06-15 1:06-15 2:06-16 5:06-19 8:06-24 
Mon -8:06-03 -5:06-08 -2:06-11 -1:06-12 0:06-15 1:06-16 2:06-17 5:06-22 8:06-25 
Tue -8:06-04 -5:06-09 -2:06-12 -1:06-15 0:06-16 1:06-17 2:06-18 5:06-23 8:06-26 
Wed -8:06-05 -5:06-10 -2:06-15 -1:06-16 0:06-17 1:06-18 2:06-19 5:06-24 8:06-29 
Thu -8:06-08 -5:06-11 -2:06-16 -1:06-17 0:06-18 1:06-19 2:06-22 5:06-25 8:06-30 
Fri -8:06-09 -5:06-12 -2:06-17 -1:06-18 0:06-19 1:06-22 2:06-23 5:06-26 8:07-01 
Sat -8:06-10 -5:06-15 -2:06-18 -1:06-19 0:06-22 1:06-22 2:06-23 5:06-26 8:07-01 
--- and how it combines
'weekday'                              2020-06-15 00:00:00
'weekdays'                             2020-06-15 00:00:00
'this weekday'                         2020-06-15 00:00:00
'next weekday'                         2020-06-16 00:00:00
'last weekday'                         2020-06-12 00:00:00
'2 weekday'                            2020-06-17 12:30:45
'+20 weekday -5 weekdays'              2020-06-08 12:30:45
'last weekday +22 sec -8 months'       2019-10-14 00:00:22
'first day of next month +9 weekdays'  2020-07-14 12:30:45
'last day of this month -11 weekdays'  2020-06-15 12:30:45
'yesterday +28 weekdays ago'           2020-05-07 00:00:00
'last week weekday'                    2020-06-08 00:00:00
'this week weekday'                    2020-06-15 00:00:00
