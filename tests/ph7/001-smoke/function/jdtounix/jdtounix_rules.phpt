--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
jdtounix() is arithmetic, so its answer is midnight UTC
--FILE--
<?php
/* jdtounix() is the bridge from the day counter to Unix time: it subtracts the
 * serial day number of 1 January 1970 and multiplies by a day's seconds. There
 * is no calendar and no clock in it, so the answer is always midnight UTC --
 * and a day before the epoch is a ValueError whose wording, unusually for this
 * extension, names neither the function nor the argument. */
function jdtounix_rules(): void {
    echo "## the epoch and the days around it\n";
    foreach ([2440588, 2440589, 2440590, 2447893] as $jd) {
        printf("%-9d %11d %s\n", $jd, jdtounix($jd), gmdate('Y-m-d H:i:s', jdtounix($jd)));
    }
    echo "## it is exactly the inverse of gregoriantojd() over the epoch\n";
    $bad = 0;
    for ($jd = 2440588; $jd < 2470000; $jd += 13) {
        [$m, $d, $y] = array_map('intval', explode('/', jdtogregorian($jd)));
        if (jdtounix($jd) !== gmmktime(0, 0, 0, $m, $d, $y)) { $bad++; }
    }
    printf("disagreements with gmmktime(): %d\n", $bad);
    echo "## before the epoch, and past what a timestamp can hold\n";
    foreach ([2440587, 0, -1, 106751993607888, 106751993607889, PHP_INT_MAX, PHP_INT_MIN] as $jd) {
        try { printf("%-21d %d\n", $jd, jdtounix($jd)); }
        catch (ValueError $e) { printf("%-21d %s\n", $jd, $e->getMessage()); }
    }
}
jdtounix_rules();
--EXPECT--
## the epoch and the days around it
2440588             0 1970-01-01 00:00:00
2440589         86400 1970-01-02 00:00:00
2440590        172800 1970-01-03 00:00:00
2447893     631152000 1990-01-01 00:00:00
## it is exactly the inverse of gregoriantojd() over the epoch
disagreements with gmmktime(): 0
## before the epoch, and past what a timestamp can hold
2440587               jday must be between 2440588 and 106751993607888
0                     jday must be between 2440588 and 106751993607888
-1                    jday must be between 2440588 and 106751993607888
106751993607888       9223372036854720000
106751993607889       jday must be between 2440588 and 106751993607888
9223372036854775807   jday must be between 2440588 and 106751993607888
-9223372036854775808  jday must be between 2440588 and 106751993607888
