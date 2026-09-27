--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
createFromTimestamp(): the float door onto the clock
--FILE--
<?php
/* php 8.4's factory was undefined on both date classes -- the only one that
 * reads MICROSECONDS out of its argument, and the only door with php's
 * DateRangeError for a timestamp no date can hold. */
date_default_timezone_set('UTC');
foreach ([0, 1, -1, PHP_INT_MAX, PHP_INT_MIN, 1.5, -1.5, 0.5, -0.5, 1.0E-6, -1.0E-6,
          1.9999999, -1.9999999, '5', '5.5', true, false] as $cftTs) {
    try {
        printf("%-22s %s\n", var_export($cftTs, true),
            DateTime::createFromTimestamp($cftTs)->format('Y-m-d H:i:s.u e'));
    } catch (Throwable $e) {
        printf("%-22s %s: %s\n", var_export($cftTs, true), get_class($e), $e->getMessage());
    }
}
/* the range is php's own, and 2^63 itself is out of it while -2^63 is in */
foreach ([9.2233720368547758E+18, -9.2233720368547758E+18, 1.0E+19, -1.0E+19,
          NAN, INF, -INF] as $cftTs) {
    try {
        printf("%-22s %s\n", var_export($cftTs, true),
            DateTime::createFromTimestamp($cftTs)->format('Y-m-d H:i:s.u'));
    } catch (Throwable $e) {
        printf("%-22s %s: %s\n", var_export($cftTs, true), get_class($e), $e->getMessage());
    }
}
/* the class built is the one the call was made THROUGH */
class CftSub extends DateTime {}
class CftSubImm extends DateTimeImmutable {}
foreach (['DateTime', 'DateTimeImmutable', 'CftSub', 'CftSubImm'] as $cftCls) {
    echo $cftCls, ' -> ', get_class($cftCls::createFromTimestamp(0)), "\n";
}
/* ...and the zone is a fixed +00:00 whatever the default is, like '@0' */
date_default_timezone_set('GMT');
echo 'GMT default: ', DateTime::createFromTimestamp(0)->format('e T P'), "\n";
date_default_timezone_set('UTC');
foreach ([[], [0, 1]] as $cftArgs) {
    try {
        var_dump(DateTime::createFromTimestamp(...$cftArgs));
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
try {
    DateTime::createFromTimestamp('abc');
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
0                      1970-01-01 00:00:00.000000 +00:00
1                      1970-01-01 00:00:01.000000 +00:00
-1                     1969-12-31 23:59:59.000000 +00:00
9223372036854775807    292277026596-12-04 15:30:07.000000 +00:00
-9223372036854775807-1 -292277022657-01-27 08:29:52.000000 +00:00
1.5                    1970-01-01 00:00:01.500000 +00:00
-1.5                   1969-12-31 23:59:58.500000 +00:00
0.5                    1970-01-01 00:00:00.500000 +00:00
-0.5                   1969-12-31 23:59:59.500000 +00:00
1.0E-6                 1970-01-01 00:00:00.000001 +00:00
-1.0E-6                1969-12-31 23:59:59.999999 +00:00
1.9999999              1970-01-01 00:00:02.000000 +00:00
-1.9999999             1969-12-31 23:59:58.000000 +00:00
'5'                    1970-01-01 00:00:05.000000 +00:00
'5.5'                  1970-01-01 00:00:05.500000 +00:00
true                   1970-01-01 00:00:01.000000 +00:00
false                  1970-01-01 00:00:00.000000 +00:00
9.223372036854776E+18  DateRangeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be a finite number between -9223372036854775808 and 9223372036854775807.999999, 9.22337e+18 given
-9.223372036854776E+18 -292277022657-01-27 08:29:52.000000
1.0E+19                DateRangeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be a finite number between -9223372036854775808 and 9223372036854775807.999999, 1.0e+19 given
-1.0E+19               DateRangeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be a finite number between -9223372036854775808 and 9223372036854775807.999999, -1.0e+19 given
NAN                    DateRangeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be a finite number between -9223372036854775808 and 9223372036854775807.999999, NAN given
INF                    DateRangeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be a finite number between -9223372036854775808 and 9223372036854775807.999999, INF given
-INF                   DateRangeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be a finite number between -9223372036854775808 and 9223372036854775807.999999, -INF given
DateTime -> DateTime
DateTimeImmutable -> DateTimeImmutable
CftSub -> CftSub
CftSubImm -> CftSubImm
GMT default: +00:00 GMT+0000 +00:00
ArgumentCountError: DateTime::createFromTimestamp() expects exactly 1 argument, 0 given
ArgumentCountError: DateTime::createFromTimestamp() expects exactly 1 argument, 2 given
TypeError: DateTime::createFromTimestamp(): Argument #1 ($timestamp) must be of type int|float, string given
--CLEAN--
<?php
/* the smoke corpus runs in ONE interpreter: leave no globals behind */
unset($cftTs, $cftCls, $cftArgs, $e);
