--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a RELATIVE number wider than thirteen digits answers the scanner's position (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* What php answers past thirteen digits, pinned so the divergence is a measured
 * thing rather than a claim: the run is not read as a number at all. A 14-digit
 * run of nines comes back as ten digits' worth, an 18-digit one as a timestamp
 * off a different part of the string, and `createFromDateString` and
 * `strtotime` disagree with `modify()` about the same text. PHL refuses every
 * one of them -- see the PHL half. */
date_default_timezone_set('UTC');
foreach ([12, 13, 14, 18, 20] as $n) {
    $run = str_repeat('9', $n);
    try {
        $d = new DateTime('@0');
        $d->modify("+$run seconds");
        printf("%2d digits  U=%s\n", $n, $d->format('U'));
    } catch (Throwable $e) {
        printf("%2d digits  %s: %s\n", $n, get_class($e), $e->getMessage());
    }
    try {
        printf("           interval d=%s\n", var_export(DateInterval::createFromDateString("$run days")->d, true));
    } catch (Throwable $e) {
        printf("           interval %s: %s\n", get_class($e), $e->getMessage());
    }
    printf("           strtotime %s\n", var_export(@strtotime("$run days", 0), true));
}
?>
--EXPECT--
12 digits  U=999999999999
           interval d=999999999999
           strtotime 86399999999913600
13 digits  U=9999999999999
           interval d=9999999999999
           strtotime 863999999999913600
14 digits  U=999999999999
           interval d=9999999999
           strtotime 864253370678400
18 digits  U=1253370764799
           interval d=9999999999
           strtotime 864253370678400
20 digits  U=263370764799
           interval d=999999999999
           strtotime 86400253370678400
