--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a RELATIVE number wider than thirteen digits is refused (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's relative-number scanner reads thirteen digits and then loses its place:
 * a 14-digit run comes back as ten digits' worth, a 15-digit one as eleven, and
 * the cycle repeats -- see the zend twin, which pins those answers. They are the
 * scanner's position, not a value anyone can mean, so PHL refuses the run
 * instead of guessing at it, with php's own "Number out of range" reason at the
 * first digit. Thirteen digits and under are identical in both engines. */
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
14 digits  DateMalformedStringException: DateTime::modify(): Failed to parse time string (+99999999999999 seconds) at position 1 (9): Number out of range
           interval DateMalformedIntervalStringException: Unknown or bad format (99999999999999 days) at position 0 (9): Number out of range
           strtotime false
18 digits  DateMalformedStringException: DateTime::modify(): Failed to parse time string (+999999999999999999 seconds) at position 1 (9): Number out of range
           interval DateMalformedIntervalStringException: Unknown or bad format (999999999999999999 days) at position 0 (9): Number out of range
           strtotime false
20 digits  DateMalformedStringException: DateTime::modify(): Failed to parse time string (+99999999999999999999 seconds) at position 1 (9): Number out of range
           interval DateMalformedIntervalStringException: Unknown or bad format (99999999999999999999 days) at position 0 (9): Number out of range
           strtotime false
