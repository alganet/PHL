--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
date_parse_from_format(): the format scanner's own components
--FILE--
<?php
/* date_parse_from_format(): the FORMAT scanner's components, through the very
 * presenter date_parse() answers with. A field the scan never filled is
 * `false` rather than a value borrowed from the clock -- which is the whole
 * point of the reader -- `is_localtime` says a zone was READ whether or not it
 * meant anything, and the `relative` block appears for exactly one reason here:
 * a textual DAY, which this parser records as a weekday to move to rather than
 * as a day of any month.
 *
 * Nothing it collects reaches getLastErrors(): php leaves that record to the
 * constructors, so the diagnostics are the call's own.
 *
 * Two answers below are ones this view is the first door wide enough to show.
 * A fraction is scaled by the bytes the READER walked, and php does that with a
 * power of ten -- so a reader that walked MORE than its own width answers a
 * truncated fraction of what it read rather than nothing, and both spellings
 * land on the same scale because the millisecond is multiplied by a thousand
 * after being divided by its width. And a zone specifier that resolves NOTHING
 * still writes the offset -- php assigns its reader's answer, which is zero --
 * while leaving the kind and the name a zone before it did resolve exactly
 * where they were: a second `T` behind a first does not unsay it. */
$ffRows = [
    ['Y-m-d', '2020-01-02'], ['Y-m-d H:i:s', '2020-01-02 03:04:05'],
    ['Fi', 'January28'], ['s', '07'], ['i', '7'], ['m', '13'], ['H', '30'],
    ['!', ''], ['|', ''], ['Y|', '2020'], ['Y!', '2020'],
    ['u', '123'], ['v', '5'], ['U', '1000000000'], ['U', '-5'],
    ['D', 'mon'], ['l', 'sunday'], ['D', 'week'], ['D', 'weekday'], ['l', 'sec'],
    ['e', 'UTC'], ['e', 'utc'], ['e', 'GMT'], ['e', 'Z'], ['e', 'A'],
    ['e', '+02:00'], ['e', '-5'], ['e', 'XYZ'], ['e', '+'],
    ['z', '100'], ['Yz', '2020100'], ['x', '+12345'], ['X', '-99'],
    ['h', '13'], ['a', 'am'], ['ha', '1pm'], ['S', 'st'],
    ['Y-m-d', '2020-02-31'], ['H:i:s', '24:00:00'],
    ['q', 'r'], ['Y', 'abc'], ['d', 'ab1'], ['Yq', '2020'], ['Y', '2020xx'],
    ['+Y', '2020xx'],
    ['u', 'x1'], ['u', 'xx1'], ['u', 'xxxxxxx1'], ['u', 'week12'], ['u', 'a.m.1234'],
    ['v', 'x1'], ['v', 'q+02:00'], ['v', 'week12'], ['v', '-(12'], ['v', 'xxxxxx1'],
    ['eT', 'UTC.'], ['eT', 'GMT.'], ['ee', 'Z.'], ['Te', '+02:00.'],
    ['eTT', 'M..'], ['ee', 'UTC+'],
];
foreach ($ffRows as $ffRow) {
    $ffOut = date_parse_from_format($ffRow[0], $ffRow[1]);
    ksort($ffOut['warnings']);
    ksort($ffOut['errors']);
    printf("%-8s %-14s %s\n", '[' . $ffRow[0] . ']', '[' . $ffRow[1] . ']',
        json_encode($ffOut));
}
/* php's PATH-string rule: the datetime a FORMAT is read against refuses a NUL
 * at every door that takes one, and only there -- the doors that take an
 * ordinary date STRING read up to the NUL and parse what is in front of it. */
$ffNul = "20\0" . '20';
$ffDoors = [
    'new DateTime'                        => fn() => new DateTime($ffNul),
    'DateTime::createFromFormat'          => fn() => DateTime::createFromFormat('Y', $ffNul),
    'DateTimeImmutable::createFromFormat' => fn() => DateTimeImmutable::createFromFormat('Y', $ffNul),
    'date_create_from_format'             => fn() => date_create_from_format('Y', $ffNul),
    'date_create_immutable_from_format'   => fn() => date_create_immutable_from_format('Y', $ffNul),
    'date_create'                         => fn() => date_create($ffNul),
    'date_parse'                          => fn() => date_parse($ffNul),
    'date_parse_from_format'              => fn() => date_parse_from_format('Y', $ffNul),
];
foreach ($ffDoors as $ffName => $ffFn) {
    echo str_pad($ffName, 38);
    try {
        $ffR = $ffFn();
        echo 'no throw: ', get_debug_type($ffR);
    } catch (Throwable $ffE) {
        echo get_class($ffE), ': ', $ffE->getMessage();
    }
    echo "\n";
}
?>
--EXPECT--
[Y-m-d]  [2020-01-02]   {"year":2020,"month":1,"day":2,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[Y-m-d H:i:s] [2020-01-02 03:04:05] {"year":2020,"month":1,"day":2,"hour":3,"minute":4,"second":5,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[Fi]     [January28]    {"year":false,"month":1,"day":false,"hour":0,"minute":28,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[s]      [07]           {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":7,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[i]      [7]            {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["A two digit minute could not be found"],"is_localtime":false}
[m]      [13]           {"year":false,"month":13,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[H]      [30]           {"year":false,"month":false,"day":false,"hour":30,"minute":0,"second":0,"fraction":0,"warning_count":1,"warnings":{"2":"The parsed time was invalid"},"error_count":0,"errors":[],"is_localtime":false}
[!]      []             {"year":1970,"month":1,"day":1,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[|]      []             {"year":1970,"month":1,"day":1,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[Y|]     [2020]         {"year":2020,"month":1,"day":1,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[Y!]     [2020]         {"year":1970,"month":1,"day":1,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[u]      [123]          {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0.123,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[v]      [5]            {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0.5,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[U]      [1000000000]   {"year":2001,"month":9,"day":9,"hour":1,"minute":46,"second":40,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false}
[U]      [-5]           {"year":1969,"month":12,"day":31,"hour":23,"minute":59,"second":55,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false}
[D]      [mon]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":1}}
[l]      [sunday]       {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":0}}
[D]      [week]         {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":7}}
[D]      [weekday]      {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":1}}
[l]      [sec]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":1}}
[e]      [UTC]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":3,"tz_abbr":"UTC","tz_id":"UTC"}
[e]      [utc]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":2,"zone":0,"is_dst":false,"tz_abbr":"UTC"}
[e]      [GMT]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":2,"zone":0,"is_dst":false,"tz_abbr":"GMT"}
[e]      [Z]            {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":2,"zone":0,"is_dst":false,"tz_abbr":"Z"}
[e]      [A]            {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":2,"zone":3600,"is_dst":false,"tz_abbr":"A"}
[e]      [+02:00]       {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":7200,"is_dst":false}
[e]      [-5]           {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":-18000,"is_dst":false}
[e]      [XYZ]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["The timezone could not be found in the database"],"is_localtime":true,"zone_type":0}
[e]      [+]            {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["The timezone could not be found in the database"],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false}
[z]      [100]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["A 'day of year' can only come after a year has been found"],"is_localtime":false}
[Yz]     [2020100]      {"year":2020,"month":4,"day":10,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[x]      [+12345]       {"year":12345,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[X]      [-99]          {"year":-99,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[h]      [13]           {"year":false,"month":false,"day":false,"hour":13,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":1,"errors":["Hour cannot be higher than 12"],"is_localtime":false}
[a]      [am]           {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["Meridian can only come after an hour has been found"],"is_localtime":false}
[ha]     [1pm]          {"year":false,"month":false,"day":false,"hour":13,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[S]      [st]           {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
[Y-m-d]  [2020-02-31]   {"year":2020,"month":2,"day":31,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":1,"warnings":{"10":"The parsed date was invalid"},"error_count":0,"errors":[],"is_localtime":false}
[H:i:s]  [24:00:00]     {"year":false,"month":false,"day":false,"hour":24,"minute":0,"second":0,"fraction":0,"warning_count":1,"warnings":{"8":"The parsed time was invalid"},"error_count":0,"errors":[],"is_localtime":false}
[q]      [r]            {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["The format separator does not match"],"is_localtime":false}
[Y]      [abc]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":2,"errors":["A four digit year could not be found"],"is_localtime":false}
[d]      [ab1]          {"year":false,"month":false,"day":1,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[Yq]     [2020]         {"year":2020,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Not enough data available to satisfy format"},"is_localtime":false}
[Y]      [2020xx]       {"year":2020,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Trailing data"},"is_localtime":false}
[+Y]     [2020xx]       {"year":2020,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":1,"warnings":{"4":"Trailing data"},"error_count":0,"errors":[],"is_localtime":false}
[u]      [x1]           {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0.01,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[u]      [xx1]          {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0.001,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[u]      [xxxxxxx1]     {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[u]      [week12]       {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":1.2e-5,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[u]      [a.m.1234]     {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":1.2e-5,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[v]      [x1]           {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0.01,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[v]      [q+02:00]      {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0.0002,"warning_count":0,"warnings":[],"error_count":2,"errors":{"0":"Unexpected data found.","4":"Trailing data"},"is_localtime":false}
[v]      [week12]       {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":1.2e-5,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[v]      [-(12]         {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0.0012,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[v]      [xxxxxx1]      {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":1,"errors":["Unexpected data found."],"is_localtime":false}
[eT]     [UTC.]         {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":2,"errors":{"3":"Trailing data"},"is_localtime":true,"zone_type":3,"tz_abbr":"UTC","tz_id":"UTC"}
[eT]     [GMT.]         {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":2,"errors":{"3":"Trailing data"},"is_localtime":true,"zone_type":2,"zone":0,"is_dst":false,"tz_abbr":"GMT"}
[ee]     [Z.]           {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":2,"errors":{"1":"Trailing data"},"is_localtime":true,"zone_type":2,"zone":0,"is_dst":false,"tz_abbr":"Z"}
[Te]     [+02:00.]      {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":2,"errors":{"6":"Trailing data"},"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false}
[eTT]    [M..]          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":3,"errors":{"1":"Trailing data"},"is_localtime":true,"zone_type":2,"zone":0,"is_dst":false,"tz_abbr":"M"}
[ee]     [UTC+]         {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":2,"errors":{"0":"The timezone could not be found in the database","4":"Not enough data available to satisfy format"},"is_localtime":true,"zone_type":0}
new DateTime                          DateMalformedStringException: Failed to parse time string (20) at position 0 (2): Unexpected character
DateTime::createFromFormat            ValueError: DateTime::createFromFormat(): Argument #2 ($datetime) must not contain any null bytes
DateTimeImmutable::createFromFormat   ValueError: DateTimeImmutable::createFromFormat(): Argument #2 ($datetime) must not contain any null bytes
date_create_from_format               ValueError: date_create_from_format(): Argument #2 ($datetime) must not contain any null bytes
date_create_immutable_from_format     ValueError: date_create_immutable_from_format(): Argument #2 ($datetime) must not contain any null bytes
date_create                           no throw: bool
date_parse                            no throw: array
date_parse_from_format                ValueError: date_parse_from_format(): Argument #2 ($datetime) must not contain any null bytes
