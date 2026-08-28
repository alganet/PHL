--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DateTime::createFromFormat + getLastErrors
--FILE--
<?php
$tz = new DateTimeZone('UTC');
echo DateTime::createFromFormat('Y-m-d H:i:s', '2024-01-15 10:30:45', $tz)->format('Y-m-d H:i:s e'), "\n";
echo DateTime::createFromFormat('!d/m/Y', '15/01/2024', $tz)->format('Y-m-d H:i:s'), "\n";
echo DateTime::createFromFormat('Y-m-d|', '2024-01-15', $tz)->format('Y-m-d H:i:s'), "\n";
echo DateTime::createFromFormat('D, d M Y', 'Mon, 15 Jan 2024', $tz)->format('Y-m-d'), "\n";
echo DateTime::createFromFormat('l dS F Y', 'Monday 15th January 2024', $tz)->format('Y-m-d'), "\n";
echo DateTime::createFromFormat('!g:i A', '11:59 PM', $tz)->format('H:i:s'), "\n";
echo DateTime::createFromFormat('!g:i a', '12:01 am', $tz)->format('H:i:s'), "\n";
echo DateTime::createFromFormat('U', '-12345')->format('U e'), "\n";
echo DateTime::createFromFormat('Y-m-d P|', '2024-01-15 -03:30', $tz)->format('c e'), "\n";
echo DateTime::createFromFormat('Y-m-d O', '2024-01-15 +0230', $tz)->format('P'), "\n";
echo DateTime::createFromFormat('Y-m-d e', '2024-01-15 GMT', $tz)->format('e'), "\n";
echo DateTime::createFromFormat('Y-m-d\\TH:i', '2024-01-15T10:30', $tz)->format('Y-m-d H:i:s'), "\n";
echo DateTime::createFromFormat('Y-m-d#H', '2024-01-15;10', $tz)->format('H:i:s'), "\n";
echo get_class(DateTimeImmutable::createFromFormat('U', '1700000000')), "\n";

// failures + getLastErrors (php's timelib messages, positions, counts)
var_export(DateTime::createFromFormat('Y-m-d', 'bogus'));
echo "\n";
print_r(DateTime::getLastErrors());
var_export(DateTime::createFromFormat('Y_m', '2024/06', $tz));
echo "\n";
print_r(DateTime::getLastErrors());
var_export(DateTime::createFromFormat('Y-m-d', '2024-01-15 extra', $tz));
echo "\n";
print_r(DateTime::getLastErrors());

// '+' downgrades trailing data to a warning; validity warnings
$w = DateTime::createFromFormat('Y-m-d+', '2024-01-15 extra', $tz);
echo $w->format('Y-m-d'), "\n";
print_r(DateTime::getLastErrors());
$v = DateTime::createFromFormat('Y-m-d H:i:s', '2024-01-15 25:61:00', $tz);
echo $v->format('Y-m-d H:i:s'), "\n";
print_r(DateTime::getLastErrors());
DateTime::createFromFormat('Y-m-d', '2024-02-31', $tz);
print_r(DateTime::getLastErrors());
DateTime::createFromFormat('Y-m-d', '2024-01-15', $tz);
var_export(DateTime::getLastErrors());
echo "\n";
echo DateTime::createFromFormat('i', '30', $tz)->format('s'), "\n";
?>
--EXPECT--
2024-01-15 10:30:45 UTC
2024-01-15 00:00:00
2024-01-15 00:00:00
2024-01-15
2024-01-15
23:59:00
00:01:00
-12345 +00:00
2024-01-15T00:00:00-03:30 -03:30
+02:30
GMT
2024-01-15 10:30:00
10:00:00
DateTimeImmutable
false
Array
(
    [warning_count] => 0
    [warnings] => Array
        (
        )

    [error_count] => 3
    [errors] => Array
        (
            [0] => A four digit year could not be found
            [5] => Not enough data available to satisfy format
        )

)
false
Array
(
    [warning_count] => 0
    [warnings] => Array
        (
        )

    [error_count] => 1
    [errors] => Array
        (
            [4] => The format separator does not match
        )

)
false
Array
(
    [warning_count] => 0
    [warnings] => Array
        (
        )

    [error_count] => 1
    [errors] => Array
        (
            [10] => Trailing data
        )

)
2024-01-15
Array
(
    [warning_count] => 1
    [warnings] => Array
        (
            [10] => Trailing data
        )

    [error_count] => 0
    [errors] => Array
        (
        )

)
2024-01-16 02:01:00
Array
(
    [warning_count] => 1
    [warnings] => Array
        (
            [19] => The parsed time was invalid
        )

    [error_count] => 0
    [errors] => Array
        (
        )

)
Array
(
    [warning_count] => 1
    [warnings] => Array
        (
            [10] => The parsed date was invalid
        )

    [error_count] => 0
    [errors] => Array
        (
        )

)
false
00
--CLEAN--
<?php
unset($tz, $w, $v);
