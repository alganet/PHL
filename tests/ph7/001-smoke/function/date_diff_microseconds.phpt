--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
diff() answers the microseconds between two dates
--FILE--
<?php
/* php's diff() fills the interval's microsecond count from the two dates, and
 * PHL answered f = 0.0 for every pair: a sub-second difference vanished, and
 * with it the SIGN of one (two instants inside the same second compared equal,
 * so the later-first spelling was not inverted) and the borrow that decides
 * every other field. */
date_default_timezone_set('UTC');
$rows = [
    ['2020-01-01 00:00:00.000000', '2020-01-01 00:00:00.500000'],
    ['2020-01-01 00:00:00.500000', '2020-01-01 00:00:01.200000'],
    ['2020-01-01 00:00:00.123456', '2020-01-01 00:00:00.123456'],
    /* one microsecond short of a day: the borrow takes the whole second off the
     * later date, so the day COUNT is 0 and the clock fields are 23:59:59 */
    ['2020-01-01 00:00:00.000001', '2020-01-02 00:00:00.000000'],
    ['2020-01-01 00:00:00.000000', '2020-01-02 00:00:00.000001'],
    /* ...and the same borrow decides the asymmetric month walk under it */
    ['2020-02-29 23:59:59.999999', '2020-03-31 00:00:00.000000'],
    ['2019-03-31 00:00:00.500000', '2020-02-29 00:00:00.000000'],
    ['2020-01-31 23:59:59.999999', '2020-03-02 00:00:00.000000'],
];
foreach ($rows as [$a, $b]) {
    $dtA = new DateTime($a);
    $dtB = new DateTime($b);
    foreach ([[$dtA, $dtB], [$dtB, $dtA]] as [$dtP, $dtQ]) {
        $d = $dtP->diff($dtQ);
        printf("%s -> %s : y=%d m=%d d=%d h=%d i=%d s=%d f=%-9s inv=%d days=%-6s | %s\n",
            $dtP->format('H:i:s.u'), $dtQ->format('H:i:s.u'),
            $d->y, $d->m, $d->d, $d->h, $d->i, $d->s, var_export($d->f, true),
            $d->invert, var_export($d->days, true),
            $d->format('%R %a %h:%i:%s %f %F'));
    }
}
/* the absolute spelling drops the sign the microseconds decided */
$dtE = (new DateTime('2020-01-01 00:00:00.500000'))->diff(new DateTime('2020-01-01 00:00:00.000000'), true);
var_dump($dtE->invert, $dtE->f);
?>
--EXPECT--
00:00:00.000000 -> 00:00:00.500000 : y=0 m=0 d=0 h=0 i=0 s=0 f=0.5       inv=0 days=0      | + 0 0:0:0 500000 500000
00:00:00.500000 -> 00:00:00.000000 : y=0 m=0 d=0 h=0 i=0 s=0 f=0.5       inv=1 days=0      | - 0 0:0:0 500000 500000
00:00:00.500000 -> 00:00:01.200000 : y=0 m=0 d=0 h=0 i=0 s=0 f=0.7       inv=0 days=0      | + 0 0:0:0 700000 700000
00:00:01.200000 -> 00:00:00.500000 : y=0 m=0 d=0 h=0 i=0 s=0 f=0.7       inv=1 days=0      | - 0 0:0:0 700000 700000
00:00:00.123456 -> 00:00:00.123456 : y=0 m=0 d=0 h=0 i=0 s=0 f=0.0       inv=0 days=0      | + 0 0:0:0 0 000000
00:00:00.123456 -> 00:00:00.123456 : y=0 m=0 d=0 h=0 i=0 s=0 f=0.0       inv=0 days=0      | + 0 0:0:0 0 000000
00:00:00.000001 -> 00:00:00.000000 : y=0 m=0 d=0 h=23 i=59 s=59 f=0.999999  inv=0 days=0      | + 0 23:59:59 999999 999999
00:00:00.000000 -> 00:00:00.000001 : y=0 m=0 d=0 h=23 i=59 s=59 f=0.999999  inv=1 days=0      | - 0 23:59:59 999999 999999
00:00:00.000000 -> 00:00:00.000001 : y=0 m=0 d=1 h=0 i=0 s=0 f=1.0E-6    inv=0 days=1      | + 1 0:0:0 1 000001
00:00:00.000001 -> 00:00:00.000000 : y=0 m=0 d=1 h=0 i=0 s=0 f=1.0E-6    inv=1 days=1      | - 1 0:0:0 1 000001
23:59:59.999999 -> 00:00:00.000000 : y=0 m=1 d=1 h=0 i=0 s=0 f=1.0E-6    inv=0 days=30     | + 30 0:0:0 1 000001
00:00:00.000000 -> 23:59:59.999999 : y=0 m=1 d=1 h=0 i=0 s=0 f=1.0E-6    inv=1 days=30     | - 30 0:0:0 1 000001
00:00:00.500000 -> 00:00:00.000000 : y=0 m=10 d=28 h=23 i=59 s=59 f=0.5       inv=0 days=334    | + 334 23:59:59 500000 500000
00:00:00.000000 -> 00:00:00.500000 : y=0 m=10 d=28 h=23 i=59 s=59 f=0.5       inv=1 days=334    | - 334 23:59:59 500000 500000
23:59:59.999999 -> 00:00:00.000000 : y=0 m=0 d=30 h=0 i=0 s=0 f=1.0E-6    inv=0 days=30     | + 30 0:0:0 1 000001
00:00:00.000000 -> 23:59:59.999999 : y=0 m=1 d=1 h=0 i=0 s=0 f=1.0E-6    inv=1 days=30     | - 30 0:0:0 1 000001
int(0)
float(0.5)
--CLEAN--
<?php
/* the smoke corpus runs in ONE interpreter: leave no globals behind */
unset($rows, $a, $b, $dtA, $dtB, $dtP, $dtQ, $d, $dtE);
