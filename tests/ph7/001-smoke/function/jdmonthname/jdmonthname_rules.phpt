--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jdmonthname()'s mode picks the calendar and the spelling
--FILE--
<?php
/* jdmonthname()'s $mode picks the CALENDAR as well as the spelling, and its
 * six values are not in the order the calendar ids are: 0/1 are the Gregorian
 * short and long names, 2/3 the Julian pair, 4 the Jewish and 5 the French.
 * Anything else is the Gregorian short name. A day the chosen calendar cannot
 * place answers the EMPTY string, which is month slot 0 of every table. */
function jdmonthname_rules(): void {
    echo "## the six modes over one day\n";
    foreach ([CAL_MONTH_GREGORIAN_SHORT, CAL_MONTH_GREGORIAN_LONG,
              CAL_MONTH_JULIAN_SHORT, CAL_MONTH_JULIAN_LONG,
              CAL_MONTH_JEWISH, CAL_MONTH_FRENCH] as $mode) {
        printf("mode %d %-10s %-10s\n", $mode,
            var_export(jdmonthname(2447893, $mode), true),
            var_export(jdmonthname(2376000, $mode), true));
    }
    echo "## an unknown mode is the Gregorian short name\n";
    foreach ([-1, 6, 99, PHP_INT_MAX, PHP_INT_MIN] as $mode) {
        printf("mode %-21d %s\n", $mode, var_export(jdmonthname(2447893, $mode), true));
    }
    echo "## a day the chosen calendar cannot place is the empty string\n";
    foreach ([0, -1, PHP_INT_MAX, 347997, 2375839] as $jd) {
        printf("%-21d", $jd);
        for ($mode = 0; $mode <= 5; $mode++) { printf(" %-12s", var_export(jdmonthname($jd, $mode), true)); }
        echo "\n";
    }
    echo "## the Jewish mode reads the year's own leap/regular table\n";
    foreach ([jewishtojd(6, 1, 5784), jewishtojd(7, 1, 5784), jewishtojd(7, 1, 5785)] as $jd) {
        printf("%-10s %s\n", jdtojewish($jd), jdmonthname($jd, CAL_MONTH_JEWISH));
    }
}
jdmonthname_rules();
--EXPECT--
## the six modes over one day
mode 0 'Jan'      'Mar'     
mode 1 'January'  'March'   
mode 2 'Dec'      'Feb'     
mode 3 'December' 'February'
mode 4 'Tevet'    'Adar'    
mode 5 ''         'Ventose' 
## an unknown mode is the Gregorian short name
mode -1                    'Jan'
mode 6                     'Jan'
mode 99                    'Jan'
mode 9223372036854775807   'Jan'
mode -9223372036854775808  'Jan'
## a day the chosen calendar cannot place is the empty string
0                     ''           ''           ''           ''           ''           ''          
-1                    ''           ''           ''           ''           ''           ''          
9223372036854775807   ''           ''           ''           ''           ''           ''          
347997                'Sep'        'September'  'Oct'        'October'    ''           ''          
2375839               'Sep'        'September'  'Sep'        'September'  'Tishri'     ''          
## the Jewish mode reads the year's own leap/regular table
6/1/5784   Adar I
7/1/5784   Adar II
7/1/5785   Adar
