--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
cal_from_jd() answers nine keys, two of them Jewish-only
--FILE--
<?php
/* cal_from_jd() answers nine keys: the date as a string, its three parts, the
 * day of the week and its two names, and the month's two names. Two of them
 * are special-cased for the Jewish calendar: a serial day number BEFORE that
 * calendar starts has a NULL day of week where every other calendar still
 * names one -- the weekday is plain arithmetic on the counter and does not
 * care whether the date exists -- and its month names come from the year's own
 * leap/regular table rather than from the calendar's. */
function cal_from_jd_rules(): void {
    echo "## one day, four calendars\n";
    foreach ([CAL_GREGORIAN, CAL_JULIAN, CAL_JEWISH, CAL_FRENCH] as $c) {
        $r = cal_from_jd(2447893, $c);
        printf("cal %d %-12s dow=%s %s/%s month=%s/%s\n", $c, $r['date'],
            var_export($r['dow'], true), $r['abbrevdayname'], $r['dayname'],
            $r['abbrevmonth'], $r['monthname']);
    }
    printf("keys: %s\n", implode(',', array_keys(cal_from_jd(1, CAL_GREGORIAN))));

    echo "## a day outside the calendar still has a weekday -- except in the Jewish one\n";
    foreach ([[0, CAL_GREGORIAN], [-1, CAL_GREGORIAN], [0, CAL_JEWISH], [347997, CAL_JEWISH],
              [-1, CAL_JEWISH], [2447893, CAL_FRENCH], [PHP_INT_MAX, CAL_GREGORIAN],
              [PHP_INT_MIN, CAL_JEWISH]] as [$jd, $c]) {
        $r = cal_from_jd($jd, $c);
        printf("%-21d cal %d %-8s dow=%-4s '%s' '%s'\n", $jd, $c, $r['date'],
            var_export($r['dow'], true), $r['dayname'], $r['monthname']);
    }

    echo "## the Jewish month name follows the YEAR's own table\n";
    foreach ([jewishtojd(6, 1, 5784), jewishtojd(7, 1, 5784), jewishtojd(7, 1, 5785)] as $jd) {
        $r = cal_from_jd($jd, CAL_JEWISH);
        printf("%s %s\n", $r['date'], $r['monthname']);
    }

    echo "## the calendar id is the SECOND argument, so its wording says #2\n";
    foreach ([-1, 4, PHP_INT_MAX] as $c) {
        try { cal_from_jd(1, $c); }
        catch (ValueError $e) { printf("%-21d %s\n", $c, $e->getMessage()); }
    }
}
cal_from_jd_rules();
--EXPECT--
## one day, four calendars
cal 0 1/1/1990     dow=1 Mon/Monday month=Jan/January
cal 1 12/19/1989   dow=1 Mon/Monday month=Dec/December
cal 2 4/4/5750     dow=1 Mon/Monday month=Tevet/Tevet
cal 3 0/0/0        dow=1 Mon/Monday month=/
keys: date,month,day,year,dow,abbrevdayname,dayname,abbrevmonth,monthname
## a day outside the calendar still has a weekday -- except in the Jewish one
0                     cal 0 0/0/0    dow=1    'Monday' ''
-1                    cal 0 0/0/0    dow=0    'Sunday' ''
0                     cal 2 0/0/0    dow=NULL '' ''
347997                cal 2 0/0/0    dow=NULL '' ''
-1                    cal 2 0/0/0    dow=NULL '' ''
2447893               cal 3 0/0/0    dow=1    'Monday' ''
9223372036854775807   cal 0 0/0/0    dow=1    'Monday' ''
-9223372036854775808  cal 2 0/0/0    dow=NULL '' ''
## the Jewish month name follows the YEAR's own table
6/1/5784 Adar I
7/1/5784 Adar II
7/1/5785 Adar
## the calendar id is the SECOND argument, so its wording says #2
-1                    cal_from_jd(): Argument #2 ($calendar) must be a valid calendar ID
4                     cal_from_jd(): Argument #2 ($calendar) must be a valid calendar ID
9223372036854775807   cal_from_jd(): Argument #2 ($calendar) must be a valid calendar ID
