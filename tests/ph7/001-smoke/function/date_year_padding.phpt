--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every year token pads php's way, and a signed year parses
--FILE--
<?php
/* Three separate wrong answers about the YEAR, all of them silent:
 *  - 'c' and 'r' SPACE-padded a year below 1000 ("  50-03-04") where php pads
 *    it with zeros to four columns including the sign ("0050", "-001") -- an
 *    ordinary date, not just a BCE one;
 *  - 'Y' and 'x' padded the whole signed number to four ("-495") where php pads
 *    the ABSOLUTE value behind the sign ("-0495");
 *  - the year lived in an int, so past 2147483647 it WRAPPED: the clock at
 *    PHP_INT_MAX seconds read 219250468 here and 292277026596 in php;
 *  - and a negative or expanded ISO year did not parse at all. */
date_default_timezone_set('UTC');
$base = new DateTime('0001-01-01 00:00:00');
foreach ([-99999, -12345, -1234, -495, -100, -99, -10, -5, -1, 0, 1, 5, 50, 99, 100, 999,
          1000, 9999, 10000, 12345] as $y) {
    $d = clone $base;
    $d->setDate($y, 3, 4);
    printf("%-7d Y=%-13s y=%-4s o=%-7s X=%-13s x=%-13s c=%s | r=%s\n", $y,
        $d->format('Y'), $d->format('y'), $d->format('o'), $d->format('X'),
        $d->format('x'), $d->format('c'), $d->format('r'));
}
/* the clock's own extremes, where an int-sized year wrapped */
foreach ([PHP_INT_MAX, PHP_INT_MIN, 67768036191676799] as $ts) {
    $d = new DateTime('@0');
    $d->setTimestamp($ts);
    echo $ts, ' -> ', $d->format('Y-m-d H:i:s'), ' | ', $d->format('c'), "\n";
}
/* the five procedural doors used to break the timestamp down with the
 * platform's gmtime(), which FAILS past an int-sized year -- and each then
 * formatted the current time instead */
set_error_handler(function ($no, $msg) { echo "  WARN: $msg\n"; return true; });
foreach ([0, PHP_INT_MAX, PHP_INT_MIN, 67768036191676799] as $ts) {
    $g = getdate($ts);
    $l = localtime($ts, true);
    /* idate answers a C int, so its year WRAPS and its -1 is a warned false */
    $u = idate('U', $ts);
    printf("%-20s date=%s idate=%s getdate=%s/%s/%s [0]=%s tm_year=%s B=%s U=%s\n", $ts,
        date('Y-m-d H:i:s', $ts), var_export(idate('Y', $ts), true),
        $g['year'], $g['mon'], $g['mday'], var_export($g[0], true),
        /* the Swatch beat of an extreme stamp overflows php's own arithmetic,
         * and the wrap is the compiler's (gcc and MSVC disagree): only the
         * ordinary stamp's is pinned */
        $l['tm_year'], $ts === 0 ? idate('B', $ts) : '-', var_export($u, true));
}
restore_error_handler();
/* php's ISO year takes a SIGN, and the sign is what admits a width past four */
foreach (['-1234-03-04', '+1234-03-04', '-0001-01-01', '-12345-01-01', '+12345-01-01',
          '-123456789-01-01', '-0000-01-01', '+0000-01-01', '-1234-03-04T05:06:07Z',
          '-1234-03-04 05:06:07+02:00', '-123-03-04', '-12-03-04', '-1234-3-4'] as $s) {
    try {
        printf("%-30s %s\n", $s, (new DateTime($s))->format('Y-m-d H:i:s P'));
    } catch (Throwable $e) {
        printf("%-30s %s\n", $s, get_class($e));
    }
}
?>
--EXPECT--
-99999  Y=-99999        y=-99  o=-99999  X=-99999        x=-99999        c=-99999-03-04T00:00:00+00:00 | r=Sun, 04 Mar -99999 00:00:00 +0000
-12345  Y=-12345        y=-45  o=-12345  X=-12345        x=-12345        c=-12345-03-04T00:00:00+00:00 | r=Thu, 04 Mar -12345 00:00:00 +0000
-1234   Y=-1234         y=-34  o=-1234   X=-1234         x=-1234         c=-1234-03-04T00:00:00+00:00 | r=Fri, 04 Mar -1234 00:00:00 +0000
-495    Y=-0495         y=-95  o=-495    X=-0495         x=-0495         c=-495-03-04T00:00:00+00:00 | r=Sat, 04 Mar -495 00:00:00 +0000
-100    Y=-0100         y=00   o=-100    X=-0100         x=-0100         c=-100-03-04T00:00:00+00:00 | r=Sun, 04 Mar -100 00:00:00 +0000
-99     Y=-0099         y=-99  o=-99     X=-0099         x=-0099         c=-099-03-04T00:00:00+00:00 | r=Mon, 04 Mar -099 00:00:00 +0000
-10     Y=-0010         y=-10  o=-10     X=-0010         x=-0010         c=-010-03-04T00:00:00+00:00 | r=Sun, 04 Mar -010 00:00:00 +0000
-5      Y=-0005         y=-5   o=-5      X=-0005         x=-0005         c=-005-03-04T00:00:00+00:00 | r=Sat, 04 Mar -005 00:00:00 +0000
-1      Y=-0001         y=-1   o=-1      X=-0001         x=-0001         c=-001-03-04T00:00:00+00:00 | r=Thu, 04 Mar -001 00:00:00 +0000
0       Y=0000          y=00   o=0       X=+0000         x=0000          c=0000-03-04T00:00:00+00:00 | r=Sat, 04 Mar 0000 00:00:00 +0000
1       Y=0001          y=01   o=1       X=+0001         x=0001          c=0001-03-04T00:00:00+00:00 | r=Sun, 04 Mar 0001 00:00:00 +0000
5       Y=0005          y=05   o=5       X=+0005         x=0005          c=0005-03-04T00:00:00+00:00 | r=Fri, 04 Mar 0005 00:00:00 +0000
50      Y=0050          y=50   o=50      X=+0050         x=0050          c=0050-03-04T00:00:00+00:00 | r=Fri, 04 Mar 0050 00:00:00 +0000
99      Y=0099          y=99   o=99      X=+0099         x=0099          c=0099-03-04T00:00:00+00:00 | r=Wed, 04 Mar 0099 00:00:00 +0000
100     Y=0100          y=00   o=100     X=+0100         x=0100          c=0100-03-04T00:00:00+00:00 | r=Thu, 04 Mar 0100 00:00:00 +0000
999     Y=0999          y=99   o=999     X=+0999         x=0999          c=0999-03-04T00:00:00+00:00 | r=Mon, 04 Mar 0999 00:00:00 +0000
1000    Y=1000          y=00   o=1000    X=+1000         x=1000          c=1000-03-04T00:00:00+00:00 | r=Tue, 04 Mar 1000 00:00:00 +0000
9999    Y=9999          y=99   o=9999    X=+9999         x=9999          c=9999-03-04T00:00:00+00:00 | r=Thu, 04 Mar 9999 00:00:00 +0000
10000   Y=10000         y=00   o=10000   X=+10000        x=+10000        c=10000-03-04T00:00:00+00:00 | r=Sat, 04 Mar 10000 00:00:00 +0000
12345   Y=12345         y=45   o=12345   X=+12345        x=+12345        c=12345-03-04T00:00:00+00:00 | r=Sun, 04 Mar 12345 00:00:00 +0000
9223372036854775807 -> 292277026596-12-04 15:30:07 | 292277026596-12-04T15:30:07+00:00
-9223372036854775808 -> -292277022657-01-27 08:29:52 | -292277022657-01-27T08:29:52+00:00
67768036191676799 -> 2147485547-12-31 23:59:59 | 2147485547-12-31T23:59:59+00:00
0                    date=1970-01-01 00:00:00 idate=1970 getdate=1970/1/1 [0]=0 tm_year=70 B=41 U=0
  WARN: idate(): Unrecognized date format token
9223372036854775807  date=292277026596-12-04 15:30:07 idate=219250468 getdate=292277026596/12/4 [0]=9223372036854775807 tm_year=292277024696 B=- U=false
-9223372036854775808 date=-292277022657-01-27 08:29:52 idate=-219246529 getdate=-292277022657/1/27 [0]=-9223372036854775807-1 tm_year=-292277024557 B=- U=0
67768036191676799    date=2147485547-12-31 23:59:59 idate=-2147481749 getdate=2147485547/12/31 [0]=67768036191676799 tm_year=2147483647 B=- U=2085923199
-1234-03-04                    -1234-03-04 00:00:00 +00:00
+1234-03-04                    1234-03-04 00:00:00 +00:00
-0001-01-01                    -0001-01-01 00:00:00 +00:00
-12345-01-01                   -12345-01-01 00:00:00 +00:00
+12345-01-01                   12345-01-01 00:00:00 +00:00
-123456789-01-01               -123456789-01-01 00:00:00 +00:00
-0000-01-01                    0000-01-01 00:00:00 +00:00
+0000-01-01                    0000-01-01 00:00:00 +00:00
-1234-03-04T05:06:07Z          -1234-03-04 05:06:07 +00:00
-1234-03-04 05:06:07+02:00     -1234-03-04 05:06:07 +02:00
-123-03-04                     DateMalformedStringException
-12-03-04                      DateMalformedStringException
-1234-3-4                      DateMalformedStringException
--CLEAN--
<?php
/* the smoke corpus runs in ONE interpreter: leave no globals behind */
unset($base, $d, $y, $ts, $g, $l, $u, $s, $e);
