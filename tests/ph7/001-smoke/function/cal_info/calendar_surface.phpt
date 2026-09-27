--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/calendar is complete: eighteen functions and twenty-one constants
--FILE--
<?php
/* The whole of php's ext/calendar, name by name and constant by constant.
 * Every one of them was a fatal on ordinary php code until this engine grew
 * them, and the extension is complete now -- so extension_loaded('calendar')
 * is honest rather than a stub. */
function calendar_surface(): void {
    $fns = ['cal_days_in_month', 'cal_from_jd', 'cal_info', 'cal_to_jd',
        'easter_date', 'easter_days', 'frenchtojd', 'gregoriantojd',
        'jddayofweek', 'jdmonthname', 'jdtofrench', 'jdtogregorian',
        'jdtojewish', 'jdtojulian', 'jdtounix', 'jewishtojd', 'juliantojd',
        'unixtojd'];
    $missing = array_values(array_filter($fns, fn($f) => !function_exists($f)));
    printf("%d functions, missing: %s\n", count($fns), implode(',', $missing) ?: 'none');

    $consts = ['CAL_GREGORIAN' => 0, 'CAL_JULIAN' => 1, 'CAL_JEWISH' => 2,
        'CAL_FRENCH' => 3, 'CAL_NUM_CALS' => 4,
        'CAL_DOW_DAYNO' => 0, 'CAL_DOW_LONG' => 1, 'CAL_DOW_SHORT' => 2,
        'CAL_MONTH_GREGORIAN_SHORT' => 0, 'CAL_MONTH_GREGORIAN_LONG' => 1,
        'CAL_MONTH_JULIAN_SHORT' => 2, 'CAL_MONTH_JULIAN_LONG' => 3,
        'CAL_MONTH_JEWISH' => 4, 'CAL_MONTH_FRENCH' => 5,
        'CAL_EASTER_DEFAULT' => 0, 'CAL_EASTER_ROMAN' => 1,
        'CAL_EASTER_ALWAYS_GREGORIAN' => 2, 'CAL_EASTER_ALWAYS_JULIAN' => 3,
        'CAL_JEWISH_ADD_ALAFIM_GERESH' => 2, 'CAL_JEWISH_ADD_ALAFIM' => 4,
        'CAL_JEWISH_ADD_GERESHAYIM' => 8];
    $wrong = [];
    foreach ($consts as $name => $want) {
        if (!defined($name)) { $wrong[] = $name . '=undefined'; continue; }
        if (constant($name) !== $want) { $wrong[] = $name . '=' . var_export(constant($name), true); }
    }
    printf("%d constants, wrong: %s\n", count($consts), implode(',', $wrong) ?: 'none');
    var_dump(extension_loaded('calendar'));

    /* One date carried the whole way round: a Gregorian date to a counter, the
     * counter to the other three calendars and to a weekday, and back again. */
    $jd = cal_to_jd(CAL_GREGORIAN, 4, 5, 2026);
    printf("jd=%d dow=%s(%s) %s\n", $jd, jddayofweek($jd, CAL_DOW_LONG),
        jddayofweek($jd), jdmonthname($jd, CAL_MONTH_GREGORIAN_LONG));
    printf("gregorian=%s julian=%s jewish=%s french=%s\n",
        jdtogregorian($jd), jdtojulian($jd), jdtojewish($jd), jdtofrench($jd));
    printf("unix=%d noon-back=%d easter=%d on %s\n", jdtounix($jd),
        unixtojd(jdtounix($jd) + 43200), easter_days(2026),
        jdtogregorian(unixtojd(easter_date(2026))));
    printf("april 2026 has %d days; nisan 5786 has %d\n",
        cal_days_in_month(CAL_GREGORIAN, 4, 2026), cal_days_in_month(CAL_JEWISH, 8, 5786));
}
calendar_surface();
--EXPECT--
18 functions, missing: none
21 constants, wrong: none
bool(true)
jd=2461136 dow=Sunday(0) April
gregorian=4/5/2026 julian=3/23/2026 jewish=8/18/5786 french=0/0/0
unix=1775347200 noon-back=2461136 easter=15 on 4/5/2026
april 2026 has 30 days; nisan 5786 has 30
