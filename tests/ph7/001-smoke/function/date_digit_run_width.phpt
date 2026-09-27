--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
How WIDE a run of digits may be in a date string, and what refuses it
--FILE--
<?php
/* No date parser here bounded a run of digits, so a long one silently WRAPPED
 * the int64 it was accumulated into: `new DateTime('@99999999999999999999')`
 * answered 7766279631452241919 (a valid-looking date in the year 246 billion),
 * `new DateInterval('P99999999999999999999D')` built an interval of
 * 7766279631452241919 days, and one run over signed overflow -- undefined
 * behaviour, which the gate's UBSan run named as soon as a test reached it.
 * php bounds each grammar separately, and the two refusals are worded
 * differently: an `@epoch` stops at 18 digits with "Number out of range", an ISO
 * duration field at 12 with "Unknown or bad format". */
date_default_timezone_set('UTC');

echo "--- an ISO duration field: twelve digits\n";
for ($n = 10; $n <= 15; $n++) {
    $spec = 'P' . str_repeat('9', $n) . 'D';
    try {
        $iv = new DateInterval($spec);
        printf("%2d digits  d=%s\n", $n, var_export($iv->d, true));
    } catch (Throwable $e) {
        printf("%2d digits  %s: %s\n", $n, get_class($e), $e->getMessage());
    }
}
/* every field takes the same ceiling, and a legal one still works */
foreach (['P999999999999Y', 'P999999999999M', 'PT999999999999H', 'PT999999999999S',
          'P1Y9999999999999M', 'PT9999999999999S'] as $spec) {
    try {
        $iv = new DateInterval($spec);
        printf("%-20s y=%d m=%d d=%d h=%d i=%d s=%d\n", $spec,
            $iv->y, $iv->m, $iv->d, $iv->h, $iv->i, $iv->s);
    } catch (Throwable $e) {
        printf("%-20s %s\n", $spec, $e->getMessage());
    }
}

echo "--- an epoch: eighteen digits\n";
for ($n = 16; $n <= 21; $n++) {
    $spec = '@' . str_repeat('9', $n);
    try {
        printf("%2d digits  U=%s\n", $n, (new DateTime($spec))->format('U'));
    } catch (Throwable $e) {
        printf("%2d digits  %s: %s\n", $n, get_class($e), $e->getMessage());
    }
}
foreach (['@-999999999999999999', '@-9999999999999999999', '@999999999999999999.5'] as $spec) {
    try {
        printf("%-24s %s\n", $spec, (new DateTime($spec))->format('U.u'));
    } catch (Throwable $e) {
        printf("%-24s %s\n", $spec, $e->getMessage());
    }
}

echo "--- and a legal run still answers what it always did\n";
printf("%s | %s | %s\n",
    (new DateTime('@999999999999999999'))->format('Y-m-d H:i:s'),
    (new DateInterval('P999999999999D'))->format('%d'),
    (new DateTime('@0'))->modify('+9999999999999 seconds')->format('U'));
?>
--EXPECT--
--- an ISO duration field: twelve digits
10 digits  d=9999999999
11 digits  d=99999999999
12 digits  d=999999999999
13 digits  DateMalformedIntervalStringException: Unknown or bad format (P9999999999999D)
14 digits  DateMalformedIntervalStringException: Unknown or bad format (P99999999999999D)
15 digits  DateMalformedIntervalStringException: Unknown or bad format (P999999999999999D)
P999999999999Y       y=999999999999 m=0 d=0 h=0 i=0 s=0
P999999999999M       y=0 m=999999999999 d=0 h=0 i=0 s=0
PT999999999999H      y=0 m=0 d=0 h=999999999999 i=0 s=0
PT999999999999S      y=0 m=0 d=0 h=0 i=0 s=999999999999
P1Y9999999999999M    Unknown or bad format (P1Y9999999999999M)
PT9999999999999S     Unknown or bad format (PT9999999999999S)
--- an epoch: eighteen digits
16 digits  U=9999999999999999
17 digits  U=99999999999999999
18 digits  U=999999999999999999
19 digits  DateMalformedStringException: Failed to parse time string (@9999999999999999999) at position 0 (@): Number out of range
20 digits  DateMalformedStringException: Failed to parse time string (@99999999999999999999) at position 0 (@): Number out of range
21 digits  DateMalformedStringException: Failed to parse time string (@999999999999999999999) at position 0 (@): Number out of range
@-999999999999999999     -999999999999999999.000000
@-9999999999999999999    Failed to parse time string (@-9999999999999999999) at position 0 (@): Number out of range
@999999999999999999.5    999999999999999999.500000
--- and a legal run still answers what it always did
31688740476-10-23 01:46:39 | -727379969 | 9999999999999
