--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the fields php's FORMAT scanner leaves unset, and what fills them
--FILE--
<?php
/* php's format parser starts every field UNSET and never reads the clock while
 * it scans: what a DateTime shows is the scan's fields with the current moment
 * poured into the holes AFTERWARDS. Two rules follow that this engine did not
 * have. Naming ANY part of the clock puts the WHOLE of it at zero -- a format
 * that read only the minute is that minute past MIDNIGHT, not past the current
 * hour -- and the meridian is an ADJUSTMENT to whatever hour was read rather
 * than a twelve-hour reading of its own, so it moves an `H` just as it moves
 * an `h`. Both reset specifiers act WHERE THEY STAND: `!` overwrites what the
 * format already read, `|` only fills what nothing has.
 *
 * The two validity WARNINGS are asked of a whole component the scan filled --
 * hour, minute and second together, year, month and day together -- which is
 * why a lone month of 13 is silent and 24:00:00 is not. */
date_default_timezone_set('UTC');
$ffRows = [
    ['Fi', 'January28'], ['s', '07'], ['i', '28'], ['H', '05'],
    ['!', ''], ['|', ''], ['!Y', '2020'], ['Y!', '2020'], ['Y|', '2020'],
    ['!Y-m-d', '2020-05-06'], ['Y-m-d!', '2020-05-06'],
    ['ha', '1am'], ['ha', '12am'], ['ha', '12pm'], ['ha', '1pm'],
    ['Ha', '05am'], ['Ha', '12am'], ['Ha', '05pm'], ['Ga', '00am'],
    ['H:i:s', '23:59:59'], ['H:i:s', '24:00:00'], ['H:i:s', '25:00:00'],
    ['m', '13'], ['Y-m-d', '2020-02-29'], ['Y-m-d', '2021-02-29'],
    ['u', '5'], ['u', '000005'], ['v', '5'],
    ['Y-m-d H:i:s.u', '2020-01-02 03:04:05.678'],
    ['U', '1000000000'], ['y', '99'], ['y', '01'],
    ['Y-m-d l', '2020-01-01 Friday'], ['Y-m-d D', '2020-01-01 Wed'],
];
foreach ($ffRows as $ffRow) {
    $ffZone = new DateTimeZone('UTC');
    $ffObj = DateTime::createFromFormat($ffRow[0] . '||', $ffRow[1], $ffZone);
    $ffErr = DateTime::getLastErrors();
    printf("%-16s %-26s %-30s %s\n", '[' . $ffRow[0] . ']', '[' . $ffRow[1] . ']',
        $ffObj === false ? 'FALSE' : $ffObj->format('Y-m-d H:i:s.u P'),
        json_encode($ffErr));
}
?>
--EXPECT--
[Fi]             [January28]                1970-01-01 00:28:00.000000 +00:00 false
[s]              [07]                       1970-01-01 00:00:07.000000 +00:00 false
[i]              [28]                       1970-01-01 00:28:00.000000 +00:00 false
[H]              [05]                       1970-01-01 05:00:00.000000 +00:00 false
[!]              []                         1970-01-01 00:00:00.000000 +00:00 false
[|]              []                         1970-01-01 00:00:00.000000 +00:00 false
[!Y]             [2020]                     2020-01-01 00:00:00.000000 +00:00 false
[Y!]             [2020]                     1970-01-01 00:00:00.000000 +00:00 false
[Y|]             [2020]                     2020-01-01 00:00:00.000000 +00:00 false
[!Y-m-d]         [2020-05-06]               2020-05-06 00:00:00.000000 +00:00 false
[Y-m-d!]         [2020-05-06]               1970-01-01 00:00:00.000000 +00:00 false
[ha]             [1am]                      1970-01-01 01:00:00.000000 +00:00 false
[ha]             [12am]                     1970-01-01 00:00:00.000000 +00:00 false
[ha]             [12pm]                     1970-01-01 12:00:00.000000 +00:00 false
[ha]             [1pm]                      1970-01-01 13:00:00.000000 +00:00 false
[Ha]             [05am]                     1970-01-01 05:00:00.000000 +00:00 false
[Ha]             [12am]                     1970-01-01 00:00:00.000000 +00:00 false
[Ha]             [05pm]                     1970-01-01 17:00:00.000000 +00:00 false
[Ga]             [00am]                     1970-01-01 00:00:00.000000 +00:00 false
[H:i:s]          [23:59:59]                 1970-01-01 23:59:59.000000 +00:00 false
[H:i:s]          [24:00:00]                 1970-01-02 00:00:00.000000 +00:00 {"warning_count":1,"warnings":{"8":"The parsed time was invalid"},"error_count":0,"errors":[]}
[H:i:s]          [25:00:00]                 1970-01-02 01:00:00.000000 +00:00 {"warning_count":1,"warnings":{"8":"The parsed time was invalid"},"error_count":0,"errors":[]}
[m]              [13]                       1971-01-01 00:00:00.000000 +00:00 {"warning_count":1,"warnings":{"2":"The parsed date was invalid"},"error_count":0,"errors":[]}
[Y-m-d]          [2020-02-29]               2020-02-29 00:00:00.000000 +00:00 false
[Y-m-d]          [2021-02-29]               2021-03-01 00:00:00.000000 +00:00 {"warning_count":1,"warnings":{"10":"The parsed date was invalid"},"error_count":0,"errors":[]}
[u]              [5]                        1970-01-01 00:00:00.500000 +00:00 false
[u]              [000005]                   1970-01-01 00:00:00.000005 +00:00 false
[v]              [5]                        1970-01-01 00:00:00.500000 +00:00 false
[Y-m-d H:i:s.u]  [2020-01-02 03:04:05.678]  2020-01-02 03:04:05.678000 +00:00 false
[U]              [1000000000]               2001-09-09 01:46:40.000000 +00:00 false
[y]              [99]                       1999-01-01 00:00:00.000000 +00:00 false
[y]              [01]                       2001-01-01 00:00:00.000000 +00:00 false
[Y-m-d l]        [2020-01-01 Friday]        2020-01-03 00:00:00.000000 +00:00 false
[Y-m-d D]        [2020-01-01 Wed]           2020-01-01 00:00:00.000000 +00:00 false
