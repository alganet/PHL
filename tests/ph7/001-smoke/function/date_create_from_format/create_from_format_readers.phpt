--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the numbers and the names php's format reader reads
--FILE--
<?php
/* Every digit field of a format is read the same way: php asks whether the
 * cursor is on a digit and merely SAYS SO, then hunts for the digits anyway --
 * so `x5` under `d` is the fifth with one refusal behind it, and the refusal
 * names the byte the specifier started on rather than where the hunt ended.
 * The minute and the second are the only fields that want EXACTLY two digits;
 * the twelve-hour spellings refuse an hour above twelve and keep it; `U` and
 * the expanded year read a RUN of signs and report their two failures at
 * position 0, php's own string-scanner door.
 *
 * The names are tables rather than shapes. A month is matched as a whole WORD
 * against php's list -- the roman numerals included, which is why `F` reads `x`
 * as October -- and a textual DAY is looked up in the RELATIVE-UNIT table, so
 * `week` and `sec` are weekdays there and `janx` is no month at all. The
 * meridian hunts for its own letter across anything in the way and then wants
 * either a bare `m` or the whole `.m.`. */
date_default_timezone_set('UTC');
$ffNum = [
    ['d', '5'], ['d', '5x'], ['d', 'x5'], ['d', 'xx5'], ['d', 'abc'], ['j', '123'],
    ['i', '5'], ['i', '059'], ['i', '5x'], ['s', '59x'], ['s', '7'],
    ['h', '0'], ['h', '12'], ['h', '13'], ['h', '99'], ['g', '13'], ['H', '30'],
    ['y', '5'], ['y', '69'], ['y', '70'], ['Y', '5'], ['Y', '12345'], ['Y', '-5'],
    ['u', 'x'], ['u', '1x'], ['u', 'x1'], ['u', 'xx1'], ['v', 'x1'],
    ['U', 'abc'], ['U', '--5'], ['U', '+-5'], ['U', 'a-5'], ['U', ' 12'],
    ['U', '99999999999999999999999999'],
    ['M', 'sept'], ['M', 'iv'], ['M', 'v'], ['F', 'x'], ['M', 'JANUARY'], ['M', 'janx'],
    ['F', 'sept'], ['F', 'january'],
    ['D', 'xyz'], ['D', 'MONDAYS'], ['l', 'sun'], ['D', 'week'], ['D', 'weekday'],
    ['l', 'sec'], ['D', 'mon,'],
    ['a', 'am'], ['a', 'a.m'], ['a', 'p'], ['ha', '1a.m'], ['ha', '1 pm'],
    ['Ha', '13 am'], ['ha', '1 xx pm'],
    ['S', 'ND'], ['S', ' '], ['S', 'xy'],
];
foreach ($ffNum as $ffCase) {
    $ffObj = DateTime::createFromFormat($ffCase[0] . '||', $ffCase[1], new DateTimeZone('UTC'));
    printf("%-6s %-30s %-30s %s\n", '[' . $ffCase[0] . ']', '[' . $ffCase[1] . ']',
        $ffObj === false ? 'FALSE' : $ffObj->format('Y-m-d H:i:s.u'),
        json_encode(DateTime::getLastErrors()));
}
?>
--EXPECT--
[d]    [5]                            1970-01-05 00:00:00.000000     false
[d]    [5x]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"1":"Trailing data"}}
[d]    [x5]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[d]    [xx5]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[d]    [abc]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":["A two digit day could not be found"]}
[j]    [123]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"2":"Trailing data"}}
[i]    [5]                            FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["A two digit minute could not be found"]}
[i]    [059]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"2":"Trailing data"}}
[i]    [5x]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":["A two digit minute could not be found","Trailing data"]}
[s]    [59x]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"2":"Trailing data"}}
[s]    [7]                            FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["A two digit second could not be found"]}
[h]    [0]                            1970-01-01 00:00:00.000000     false
[h]    [12]                           1970-01-01 12:00:00.000000     false
[h]    [13]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Hour cannot be higher than 12"]}
[h]    [99]                           FALSE                          {"warning_count":1,"warnings":{"2":"The parsed time was invalid"},"error_count":1,"errors":["Hour cannot be higher than 12"]}
[g]    [13]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Hour cannot be higher than 12"]}
[H]    [30]                           1970-01-02 06:00:00.000000     {"warning_count":1,"warnings":{"2":"The parsed time was invalid"},"error_count":0,"errors":[]}
[y]    [5]                            2005-01-01 00:00:00.000000     false
[y]    [69]                           2069-01-01 00:00:00.000000     false
[y]    [70]                           1970-01-01 00:00:00.000000     false
[Y]    [5]                            0005-01-01 00:00:00.000000     false
[Y]    [12345]                        FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Trailing data"}}
[Y]    [-5]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[u]    [x]                            FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":["A six digit microsecond could not be found"]}
[u]    [1x]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"1":"Trailing data"}}
[u]    [x1]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[u]    [xx1]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[v]    [x1]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[U]    [abc]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":["Found unexpected data"]}
[U]    [--5]                          1970-01-01 00:00:05.000000     false
[U]    [+-5]                          1969-12-31 23:59:55.000000     false
[U]    [a-5]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[U]    [ 12]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."]}
[U]    [99999999999999999999999999]   FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":{"0":"Number out of range","24":"Trailing data"}}
[M]    [sept]                         1970-09-01 00:00:00.000000     false
[M]    [iv]                           1970-04-01 00:00:00.000000     false
[M]    [v]                            1970-05-01 00:00:00.000000     false
[F]    [x]                            1970-10-01 00:00:00.000000     false
[M]    [JANUARY]                      1970-01-01 00:00:00.000000     false
[M]    [janx]                         FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["A textual month could not be found"]}
[F]    [sept]                         1970-09-01 00:00:00.000000     false
[F]    [january]                      1970-01-01 00:00:00.000000     false
[D]    [xyz]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["A textual day could not be found"]}
[D]    [MONDAYS]                      1970-01-05 00:00:00.000000     false
[l]    [sun]                          1970-01-04 00:00:00.000000     false
[D]    [week]                         1970-01-04 00:00:00.000000     false
[D]    [weekday]                      1970-01-05 00:00:00.000000     false
[l]    [sec]                          1970-01-05 00:00:00.000000     false
[D]    [mon,]                         FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"3":"Trailing data"}}
[a]    [am]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Meridian can only come after an hour has been found"]}
[a]    [a.m]                          FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":["A meridian could not be found"]}
[a]    [p]                            FALSE                          {"warning_count":0,"warnings":[],"error_count":2,"errors":["A meridian could not be found"]}
[ha]   [1a.m]                         FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"1":"A meridian could not be found"}}
[ha]   [1 pm]                         1970-01-01 13:00:00.000000     false
[Ha]   [13 am]                        1970-01-01 13:00:00.000000     false
[ha]   [1 xx pm]                      1970-01-01 13:00:00.000000     false
[S]    [ND]                           1970-01-01 00:00:00.000000     false
[S]    [ ]                            FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Trailing data"]}
[S]    [xy]                           FALSE                          {"warning_count":0,"warnings":[],"error_count":1,"errors":["Trailing data"]}
