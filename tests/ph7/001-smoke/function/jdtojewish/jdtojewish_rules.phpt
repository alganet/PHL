--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jdtojewish() spells the date in numbers or in Hebrew numerals
--FILE--
<?php
/* The Hebrew spelling is a numeral system, not a transliteration: letters
 * stand for 1-9, 10-90 and 100-400, a thousands digit is written first, and 15
 * and 16 are spelled 9+6 and 9+7 so the divine name's two letters are never
 * written. It is deliberately NOT unique -- year 5 and year 5000 both come out
 * as a single he -- which is why php's own comment says to use the numeric
 * form for calculations. The bytes are ISO-8859-8, so they are shown as hex. */
function cal_jdtojewish_rules(): void {
    echo "## the numeric spelling, and the days outside the calendar\n";
    foreach ([347997, 347998, 347999, 2440588, 2447893, 324542846, 324542847, 0, -1] as $jd) {
        printf("%-9d %s\n", $jd, jdtojewish($jd));
    }
    echo "## the Hebrew spelling and its three flags\n";
    foreach ([0, CAL_JEWISH_ADD_ALAFIM_GERESH, CAL_JEWISH_ADD_ALAFIM,
              CAL_JEWISH_ADD_GERESHAYIM,
              CAL_JEWISH_ADD_ALAFIM_GERESH | CAL_JEWISH_ADD_GERESHAYIM,
              CAL_JEWISH_ADD_ALAFIM | CAL_JEWISH_ADD_GERESHAYIM] as $flags) {
        printf("flags %-2d %s\n", $flags, bin2hex(jdtojewish(2447893, true, $flags)));
    }
    echo "## 15 and 16 are never spelled with the divine name's letters\n";
    foreach ([2447904, 2447905, 2447906] as $jd) {
        printf("%s = %s\n", jdtojewish($jd), bin2hex(jdtojewish($jd, true)));
    }
    echo "## a leap year names Adar I and Adar II; a regular year says Adar\n";
    foreach ([jewishtojd(6, 1, 5784), jewishtojd(7, 1, 5784), jewishtojd(7, 1, 5785)] as $jd) {
        printf("%s = %s\n", jdtojewish($jd), bin2hex(jdtojewish($jd, true)));
    }
    echo "## the Hebrew spelling refuses a year it has no numeral for\n";
    foreach ([0, 347997, 7000000] as $jd) {
        try { jdtojewish($jd, true); }
        catch (ValueError $e) { printf("%-9d %s\n", $jd, $e->getMessage()); }
    }
}
cal_jdtojewish_rules();
--EXPECT--
## the numeric spelling, and the days outside the calendar
347997    0/0/0
347998    1/1/1
347999    1/2/1
2440588   4/23/5730
2447893   4/4/5750
324542846 12/13/887605
324542847 0/0/0
0         0/0/0
-1        0/0/0
## the Hebrew spelling and its three flags
flags 0  e320e8e1fa20e4faf9f0
flags 2  e320e8e1fa20e427faf9f0
flags 4  e320e8e1fa20e420e0ecf4e9ed20faf9f0
flags 8  e32720e8e1fa20e4faf922f0
flags 10 e32720e8e1fa20e427faf922f0
flags 12 e32720e8e1fa20e420e0ecf4e9ed20faf922f0
## 15 and 16 are never spelled with the divine name's letters
4/15/5750 = e8e520e8e1fa20e4faf9f0
4/16/5750 = e8e620e8e1fa20e4faf9f0
4/17/5750 = e9e620e8e1fa20e4faf9f0
## a leap year names Adar I and Adar II; a regular year says Adar
6/1/5784 = e020e0e3f820e02720e4faf9f4e3
7/1/5784 = e020e0e3f820e12720e4faf9f4e3
7/1/5785 = e020e0e3f820e4faf9f4e4
## the Hebrew spelling refuses a year it has no numeral for
0         Year out of range (0-9999)
347997    Year out of range (0-9999)
7000000   Year out of range (0-9999)
