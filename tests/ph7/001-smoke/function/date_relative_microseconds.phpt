--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The relative parser moves the clock by microseconds and milliseconds
--FILE--
<?php
/* The relative-time parser had no SUB-SECOND unit at all, so every spelling php
 * understands was a parse failure: `DateInterval::createFromDateString('500
 * microseconds')` threw, and `modify('+1 msec')` threw. php reads six words for
 * the microsecond and four for the millisecond, and the two engines must also
 * agree on the three it does NOT read (`us`, the Greek mu, a fractional number).
 *
 * The two doors carry it differently, which is php's model, not an accident: an
 * INTERVAL keeps the count beside its six fields and never carries it into the
 * seconds (`1000000 microseconds` is an interval of exactly that many, whose `s`
 * is 0), while the CLOCK carries once and FLOORS, so a negative run borrows a
 * whole second. */
date_default_timezone_set('UTC');

echo "--- as an interval\n";
foreach (['1 microsecond', '2 microseconds', '500 microseconds', '1 usec', '3 usecs',
          '1 µs', '5 µs', '1 msec', '2 msecs', '1 millisecond', '7 milliseconds',
          '1 ms', '250 ms', '+1 microsecond', '-1 microsecond', '0 microseconds',
          '1 microseconds', '1000000 microseconds', '1 second 5 microseconds',
          '2 days 3 hours 4 msecs', '-1 hour -1 ms'] as $spec) {
    $iv = DateInterval::createFromDateString($spec);
    printf("%-24s y=%d m=%d d=%d h=%d i=%d s=%d f=%s | %%f=%s\n", "'$spec'",
        $iv->y, $iv->m, $iv->d, $iv->h, $iv->i, $iv->s, var_export($iv->f, true),
        $iv->format('%f'));
}

echo "--- and on the clock\n";
foreach (['+1 microsecond', '-1 microsecond', '-500 microseconds', '+1 msec', '+1 ms',
          '+250 milliseconds', '+1 µs', '+1 usec', '+999999 microseconds',
          '+1000000 microseconds', '-1000001 microseconds', '+1 day 1 ms'] as $spec) {
    $d = new DateTime('2020-01-01 00:00:00.000000');
    $d->modify($spec);
    printf("%-26s %s\n", "'$spec'", $d->format('Y-m-d H:i:s.u'));
    /* the immutable twin answers a copy, and the receiver keeps its own clock */
    $i = new DateTimeImmutable('2020-01-01 00:00:00.000400');
    printf("%-26s %s (receiver %s)\n", '', $i->modify($spec)->format('H:i:s.u'),
        $i->format('H:i:s.u'));
}

echo "--- a modifier that does NOT name microseconds leaves them alone\n";
$keep = new DateTime('2020-01-01 00:00:00.123456');
$keep->modify('+1 day');
echo $keep->format('Y-m-d H:i:s.u'), "\n";
$keep->modify('+2 hours +3 seconds');
echo $keep->format('Y-m-d H:i:s.u'), "\n";
/* ...but a time of day in the string REPLACES them, fraction and all */
$keep->modify('05:06:07.000009');
echo $keep->format('Y-m-d H:i:s.u'), "\n";
$keep->modify('08:09:10');
echo $keep->format('Y-m-d H:i:s.u'), "\n";

echo "--- the spellings php does not read\n";
foreach (['1 us', '5 us'] as $spec) {
    try {
        $iv = DateInterval::createFromDateString($spec);
        printf("%-12s built f=%s\n", "'$spec'", var_export($iv->f, true));
    } catch (Throwable $e) {
        printf("%-12s %s\n", "'$spec'", get_class($e));
    }
}

echo "--- an interval built this way still moves a date\n";
foreach (['500 microseconds', '1 msec', '1 second 250 ms', '-750 microseconds'] as $spec) {
    $iv = DateInterval::createFromDateString($spec);
    $base = new DateTime('2020-01-01 00:00:00.000600');
    $plus = (clone $base)->add($iv);
    $minus = (clone $base)->sub($iv);
    printf("%-20s add=%s sub=%s\n", "'$spec'", $plus->format('H:i:s.u'), $minus->format('H:i:s.u'));
}
?>
--EXPECT--
--- as an interval
'1 microsecond'          y=0 m=0 d=0 h=0 i=0 s=0 f=1.0E-6 | %f=1
'2 microseconds'         y=0 m=0 d=0 h=0 i=0 s=0 f=2.0E-6 | %f=2
'500 microseconds'       y=0 m=0 d=0 h=0 i=0 s=0 f=0.0005 | %f=500
'1 usec'                 y=0 m=0 d=0 h=0 i=0 s=0 f=1.0E-6 | %f=1
'3 usecs'                y=0 m=0 d=0 h=0 i=0 s=0 f=3.0E-6 | %f=3
'1 µs'                  y=0 m=0 d=0 h=0 i=0 s=0 f=1.0E-6 | %f=1
'5 µs'                  y=0 m=0 d=0 h=0 i=0 s=0 f=5.0E-6 | %f=5
'1 msec'                 y=0 m=0 d=0 h=0 i=0 s=0 f=0.001 | %f=1000
'2 msecs'                y=0 m=0 d=0 h=0 i=0 s=0 f=0.002 | %f=2000
'1 millisecond'          y=0 m=0 d=0 h=0 i=0 s=0 f=0.001 | %f=1000
'7 milliseconds'         y=0 m=0 d=0 h=0 i=0 s=0 f=0.007 | %f=7000
'1 ms'                   y=0 m=0 d=0 h=0 i=0 s=0 f=0.001 | %f=1000
'250 ms'                 y=0 m=0 d=0 h=0 i=0 s=0 f=0.25 | %f=250000
'+1 microsecond'         y=0 m=0 d=0 h=0 i=0 s=0 f=1.0E-6 | %f=1
'-1 microsecond'         y=0 m=0 d=0 h=0 i=0 s=0 f=-1.0E-6 | %f=-1
'0 microseconds'         y=0 m=0 d=0 h=0 i=0 s=0 f=0.0 | %f=0
'1 microseconds'         y=0 m=0 d=0 h=0 i=0 s=0 f=1.0E-6 | %f=1
'1000000 microseconds'   y=0 m=0 d=0 h=0 i=0 s=0 f=1.0 | %f=1000000
'1 second 5 microseconds' y=0 m=0 d=0 h=0 i=0 s=1 f=5.0E-6 | %f=5
'2 days 3 hours 4 msecs' y=0 m=0 d=2 h=3 i=0 s=0 f=0.004 | %f=4000
'-1 hour -1 ms'          y=0 m=0 d=0 h=-1 i=0 s=0 f=-0.001 | %f=-1000
--- and on the clock
'+1 microsecond'           2020-01-01 00:00:00.000001
                           00:00:00.000401 (receiver 00:00:00.000400)
'-1 microsecond'           2019-12-31 23:59:59.999999
                           00:00:00.000399 (receiver 00:00:00.000400)
'-500 microseconds'        2019-12-31 23:59:59.999500
                           23:59:59.999900 (receiver 00:00:00.000400)
'+1 msec'                  2020-01-01 00:00:00.001000
                           00:00:00.001400 (receiver 00:00:00.000400)
'+1 ms'                    2020-01-01 00:00:00.001000
                           00:00:00.001400 (receiver 00:00:00.000400)
'+250 milliseconds'        2020-01-01 00:00:00.250000
                           00:00:00.250400 (receiver 00:00:00.000400)
'+1 µs'                   2020-01-01 00:00:00.000001
                           00:00:00.000401 (receiver 00:00:00.000400)
'+1 usec'                  2020-01-01 00:00:00.000001
                           00:00:00.000401 (receiver 00:00:00.000400)
'+999999 microseconds'     2020-01-01 00:00:00.999999
                           00:00:01.000399 (receiver 00:00:00.000400)
'+1000000 microseconds'    2020-01-01 00:00:01.000000
                           00:00:01.000400 (receiver 00:00:00.000400)
'-1000001 microseconds'    2019-12-31 23:59:58.999999
                           23:59:59.000399 (receiver 00:00:00.000400)
'+1 day 1 ms'              2020-01-02 00:00:00.001000
                           00:00:00.001400 (receiver 00:00:00.000400)
--- a modifier that does NOT name microseconds leaves them alone
2020-01-02 00:00:00.123456
2020-01-02 02:00:03.123456
2020-01-02 05:06:07.000009
2020-01-02 08:09:10.000000
--- the spellings php does not read
'1 us'       DateMalformedIntervalStringException
'5 us'       DateMalformedIntervalStringException
--- an interval built this way still moves a date
'500 microseconds'   add=00:00:00.001100 sub=00:00:00.000100
'1 msec'             add=00:00:00.001600 sub=23:59:59.999600
'1 second 250 ms'    add=00:00:01.250600 sub=23:59:58.750600
'-750 microseconds'  add=23:59:59.999850 sub=00:00:00.001350
