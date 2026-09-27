--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
cal_info() is the calendar table, one row or all four
--FILE--
<?php
/* cal_info() is the calendar TABLE itself: the month names each calendar has,
 * how many of them, the longest month, and the name and constant PHP knows it
 * by. The two lunisolar calendars have thirteen months and no separate
 * abbreviations -- the same table answers both keys -- and the Jewish one is
 * always shown in its LEAP-year spelling here, Adar I and Adar II both, even
 * though a regular year has neither. Called with no argument (or -1) it hands
 * back all four, keyed by calendar id. */
function cal_info_rules(): void {
    foreach ([CAL_GREGORIAN, CAL_JULIAN, CAL_JEWISH, CAL_FRENCH] as $cal) {
        $info = cal_info($cal);
        printf("%-9s %-14s months=%d maxdays=%d\n",
            $info['calname'], $info['calsymbol'],
            count($info['months']), $info['maxdaysinmonth']);
        printf("  long : %s\n", implode(',', $info['months']));
        printf("  short: %s\n", implode(',', $info['abbrevmonths']));
        printf("  keys : %s / first month key %d\n",
            implode(',', array_keys($info)), array_key_first($info['months']));
    }
    echo "## no argument is the same as -1: every calendar at once\n";
    var_dump(cal_info() === cal_info(-1));
    var_dump(array_keys(cal_info(-1)));
    var_dump(cal_info(-1)[CAL_FRENCH] === cal_info(CAL_FRENCH));
    echo "## the extension answers for itself, whatever case it is asked in\n";
    var_dump(extension_loaded('calendar'), extension_loaded('CALENDAR'),
        in_array('calendar', array_map('strtolower', get_loaded_extensions()), true));
    echo "## -1 is the only negative it takes\n";
    foreach ([-2, 4, 99, PHP_INT_MAX, PHP_INT_MIN] as $cal) {
        try { cal_info($cal); }
        catch (ValueError $e) { printf("%-21d %s\n", $cal, $e->getMessage()); }
    }
}
cal_info_rules();
--EXPECT--
Gregorian CAL_GREGORIAN  months=12 maxdays=31
  long : January,February,March,April,May,June,July,August,September,October,November,December
  short: Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec
  keys : months,abbrevmonths,maxdaysinmonth,calname,calsymbol / first month key 1
Julian    CAL_JULIAN     months=12 maxdays=31
  long : January,February,March,April,May,June,July,August,September,October,November,December
  short: Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec
  keys : months,abbrevmonths,maxdaysinmonth,calname,calsymbol / first month key 1
Jewish    CAL_JEWISH     months=13 maxdays=30
  long : Tishri,Heshvan,Kislev,Tevet,Shevat,Adar I,Adar II,Nisan,Iyyar,Sivan,Tammuz,Av,Elul
  short: Tishri,Heshvan,Kislev,Tevet,Shevat,Adar I,Adar II,Nisan,Iyyar,Sivan,Tammuz,Av,Elul
  keys : months,abbrevmonths,maxdaysinmonth,calname,calsymbol / first month key 1
French    CAL_FRENCH     months=13 maxdays=30
  long : Vendemiaire,Brumaire,Frimaire,Nivose,Pluviose,Ventose,Germinal,Floreal,Prairial,Messidor,Thermidor,Fructidor,Extra
  short: Vendemiaire,Brumaire,Frimaire,Nivose,Pluviose,Ventose,Germinal,Floreal,Prairial,Messidor,Thermidor,Fructidor,Extra
  keys : months,abbrevmonths,maxdaysinmonth,calname,calsymbol / first month key 1
## no argument is the same as -1: every calendar at once
bool(true)
array(4) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(2)
  [3]=>
  int(3)
}
bool(true)
## the extension answers for itself, whatever case it is asked in
bool(true)
bool(true)
bool(true)
## -1 is the only negative it takes
-2                    cal_info(): Argument #1 ($calendar) must be a valid calendar ID
4                     cal_info(): Argument #1 ($calendar) must be a valid calendar ID
99                    cal_info(): Argument #1 ($calendar) must be a valid calendar ID
9223372036854775807   cal_info(): Argument #1 ($calendar) must be a valid calendar ID
-9223372036854775808  cal_info(): Argument #1 ($calendar) must be a valid calendar ID
