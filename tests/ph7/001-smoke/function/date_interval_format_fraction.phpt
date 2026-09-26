--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DateInterval fraction is truncated into microseconds
--FILE--
<?php
/* DateInterval's %f and %F scale the fraction to microseconds and CAST, so the
 * token carries the whole contract of php's float->int conversion: it
 * TRUNCATES toward zero, and a fraction whose million-fold leaves the int64
 * range WRAPS the way every other cast does (a NaN or an infinity is 0).
 *
 * PHL rounded instead (0.1234567 printed 123457), saturated at PHP_INT_MIN for
 * everything out of range -- through a C cast that is undefined there, the §2
 * hazard -- and %F narrowed the result to an `int`, so any microsecond count
 * past INT_MAX printed as "000000". The assignment is suppressed because php
 * raises the cast warning THERE, where it stores the microseconds; PHL keeps
 * the float and converts on format (§7.4). */
$i = new DateInterval('PT0S');
foreach ([0.0, 0.000005, 0.00005, -0.000005, 0.5, -0.75, 0.999999, 0.1234567,
          5.0E-7, 12.5, 1.9999999, 1.0E+13, 1.0E+19, -1.0E+19, NAN, INF, -INF,
          1.0E+100] as $f) {
    @($i->f = $f);
    printf("%-14s %%f=%-21s %%F=%s\n", var_export($f, true),
        @$i->format('%f'), @$i->format('%F'));
}
/* Every microsecond value a diff can produce round-trips through the same
 * conversion, php's own floating-point artefacts included. */
$off = 0;
for ($us = 0; $us <= 999999; $us += 4999) {
    $i->f = $us / 1000000;
    if ($i->format('%f') !== (string)$us) { $off++; }
}
var_dump($off);
?>
--EXPECT--
0.0            %f=0                     %F=000000
5.0E-6         %f=5                     %F=000005
5.0E-5         %f=50                    %F=000050
-5.0E-6        %f=-5                    %F=-00005
0.5            %f=500000                %F=500000
-0.75          %f=-750000               %F=-750000
0.999999       %f=999999                %F=999999
0.1234567      %f=123456                %F=123456
5.0E-7         %f=0                     %F=000000
12.5           %f=12500000              %F=12500000
1.9999999      %f=1999999               %F=1999999
10000000000000.0 %f=-8446744073709551616  %F=-8446744073709551616
1.0E+19        %f=1590897979265384448   %F=1590897979265384448
-1.0E+19       %f=-1590897979265384448  %F=-1590897979265384448
NAN            %f=0                     %F=000000
INF            %f=0                     %F=000000
-INF           %f=0                     %F=000000
1.0E+100       %f=0                     %F=000000
int(2)
