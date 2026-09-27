--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the day-of-year, the expanded year, and php's own normalizer
--FILE--
<?php
/* Two format characters this engine had no reading for at all, and both are
 * php's rather than anyone's guess. `z` is a whole DATE rather than a field: it
 * wants a year already read -- and says so in a sentence of its own when there
 * is none -- then puts the month back at January, the day at the count plus
 * one, and NORMALIZES the vector where it stands, which is what carries a
 * 30-hour clock into the next day. `x` and `X` are the EXPANDED year, a run of
 * signs and up to nineteen digits, and the year php takes from a run it could
 * not read is zero rather than none.
 *
 * The normalizer is asked of the whole vector, unset fields included, and php
 * carries one of those into the field above it like any other number -- so a
 * format that reads a SECOND and a day-of-year and no hour publishes a date
 * eleven centuries off. That is php's answer, and it is reproduced here rather
 * than tidied.
 *
 * It is also the normalizer the RESOLVED vector goes through on its way to a
 * moment, which is where php's month carry is a CALENDAR one: the fortieth
 * month of 1970 is April 1973, not forty thirty-day steps from January. */
date_default_timezone_set('UTC');
$ffDoy = [
    ['z', '100'], ['Yz', '2020100'], ['Y z', '2020 400'], ['Y-m-d z', '2020-05-06 100'],
    ['!z', '100'], ['|z', '100'], ['zY', '1002020'], ['Y z z', '2020 100 100'],
    ['Y H:i:s z', '2020 30:00:00 100'], ['Y H:i:s z', '2020 30:70:70 100'],
    ['Y H:i z', '2020 30:70 100'], ['Y Hz', '2020 30100'],
    ['Y s z', '2020 59 100'], ['Y i z', '2020 59 100'], ['Y u z', '2020 5 100'],
    ['z', 'abc'], ['Yz', '2020abc'], ['Yz', '2020 5'],
    ['x', '+12345'], ['X', '-99'], ['x', 'abc'], ['x', '--5'], ['X', '+-3'],
    ['x', '99999999999999999999'], ['!x', '2020'], ['x-m-d', '2020-01-02'],
    ['!m', '40'], ['!m', '25'], ['!m', '99'], ['!Y-m-d', '2020-40-01'],
    ['!m-d', '40-40'], ['!d', '99'], ['!H', '99'], ['!Y-m-d H:i:s', '2020-13-40 99:99:99'],
];
foreach ($ffDoy as $ffCase) {
    $ffObj = DateTime::createFromFormat($ffCase[0] . '||', $ffCase[1], new DateTimeZone('UTC'));
    printf("%-12s %-22s %-30s %s\n", '[' . $ffCase[0] . ']', '[' . $ffCase[1] . ']',
        $ffObj === false ? 'FALSE' : $ffObj->format('Y-m-d H:i:s.u'),
        json_encode(DateTime::getLastErrors()));
}
?>
--EXPECT--
[z]          [100]                  FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["A 'day of year' can only come after a year has been found"]}
[Yz]         [2020100]              2020-04-10 00:00:00.000000     false
[Y z]        [2020 400]             2021-02-04 00:00:00.000000     false
[Y-m-d z]    [2020-05-06 100]       2020-04-10 00:00:00.000000     false
[!z]         [100]                  1970-04-11 00:00:00.000000     false
[|z]         [100]                  1970-04-11 00:00:00.000000     false
[zY]         [1002020]              FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["A 'day of year' can only come after a year has been found"]}
[Y z z]      [2020 100 100]         2020-04-10 00:00:00.000000     false
[Y H:i:s z]  [2020 30:00:00 100]    2020-04-11 06:00:00.000000     false
[Y H:i:s z]  [2020 30:70:70 100]    2020-04-11 07:11:10.000000     false
[Y H:i z]    [2020 30:70 100]       2020-04-11 07:10:00.000000     {"warning_count":1,"warnings":{"14":"The parsed time was invalid"},"error_count":0,"errors":[]}
[Y Hz]       [2020 30100]           2020-04-11 06:00:00.000000     {"warning_count":1,"warnings":{"10":"The parsed time was invalid"},"error_count":0,"errors":[]}
[Y s z]      [2020 59 100]          0860-06-18 22:21:59.000000     false
[Y i z]      [2020 59 100]          2020-04-10 00:59:00.000000     false
[Y u z]      [2020 5 100]           2020-04-10 00:00:00.500000     false
[z]          [abc]                  FALSE                          {"warning_count":0,"warnings":[],"error_count":3,"errors":["A three digit day-of-year could not be found"]}
[Yz]         [2020abc]              FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":{"4":"A three digit day-of-year could not be found"}}
[Yz]         [2020 5]               FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Unexpected data found."}}
[x]          [+12345]               12345-01-01 00:00:00.000000    false
[X]          [-99]                  -0099-01-01 00:00:00.000000    false
[x]          [abc]                  FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":["Found unexpected data"]}
[x]          [--5]                  0005-01-01 00:00:00.000000     false
[X]          [+-3]                  -0003-01-01 00:00:00.000000    false
[x]          [99999999999999999999] FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":{"0":"Number out of range","19":"Trailing data"}}
[!x]         [2020]                 2020-01-01 00:00:00.000000     false
[x-m-d]      [2020-01-02]           2020-01-02 00:00:00.000000     false
[!m]         [40]                   1973-04-01 00:00:00.000000     {"warning_count":1,"warnings":{"2":"The parsed date was invalid"},"error_count":0,"errors":[]}
[!m]         [25]                   1972-01-01 00:00:00.000000     {"warning_count":1,"warnings":{"2":"The parsed date was invalid"},"error_count":0,"errors":[]}
[!m]         [99]                   1978-03-01 00:00:00.000000     {"warning_count":1,"warnings":{"2":"The parsed date was invalid"},"error_count":0,"errors":[]}
[!Y-m-d]     [2020-40-01]           2023-04-01 00:00:00.000000     {"warning_count":1,"warnings":{"10":"The parsed date was invalid"},"error_count":0,"errors":[]}
[!m-d]       [40-40]                1973-05-10 00:00:00.000000     {"warning_count":1,"warnings":{"5":"The parsed date was invalid"},"error_count":0,"errors":[]}
[!d]         [99]                   1970-04-09 00:00:00.000000     {"warning_count":1,"warnings":{"2":"The parsed date was invalid"},"error_count":0,"errors":[]}
[!H]         [99]                   1970-01-05 03:00:00.000000     {"warning_count":1,"warnings":{"2":"The parsed time was invalid"},"error_count":0,"errors":[]}
[!Y-m-d H:i:s] [2020-13-40 99:99:99]  2021-02-13 04:40:39.000000     {"warning_count":2,"warnings":{"19":"The parsed date was invalid"},"error_count":0,"errors":[]}
