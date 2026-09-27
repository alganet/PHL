--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
date_parse(): what the scanner READ, field by field
--FILE--
<?php
/* `date_parse()` runs the same scanner every constructor runs and then SHOWS
 * what it read rather than applying it, so nothing about the clock reaches the
 * answer: a field the string never mentioned is `false`, not the base moment's.
 *
 * Three parts of the shape are conditional. `is_localtime` is whether a
 * TIMEZONE token was seen at all, which is not the same as one having been
 * understood -- an unknown name sets it and leaves `zone_type` 0. A fixed
 * OFFSET shows `zone` and `is_dst`, an ABBREVIATION shows those and its
 * `tz_abbr`, an IDENTIFIER shows only its name, twice. And the `relative` block
 * appears when the string spelled a relative element -- `now` and `today` do
 * not -- carrying the weekday when one was hunted, the business-day count when
 * that special was named, and the `first|last day of` flag as `true`.
 *
 * Two rules of php's own show only here. Its microseconds stay UNSET in the
 * nocolon arms its HAVE_TIME never runs through, so `1234` has no fraction
 * while `123456` and `t9` have one; and `datenoyear` -- a month name with a day
 * and no year -- UNSETS the year, which is what makes `@100 january 12` yearless
 * where `@100 12 january` keeps 1970. */
date_default_timezone_set('UTC');
$rows = ['2020-01-02', '2020-01-02 12:00:00.5 +02:00', '2020-01-02 12:00 UTC',
         'xyz', 'xyz abc', 'Z', '2020-13-45 25:70:80', '2020-02-31', '2020-102',
         'now', 'today', 'noon', '+1 day', '+0 day', 'tomorrow', 'yesterday',
         'next monday', 'monday', 'this week', 'next week', '+3 weekdays',
         'first day of', 'last day of', 'first monday of january', '@100', '@0',
         '@-1.5', '2020-W05', '2020-W05-3', '12:00', '3pm', '1234', '123456',
         't9', '2020', '2500', '20240102', 'jan', 'january', 'january 2020',
         'january 12', '@100 january 12', '@100 12 january', '4/20', '1.2.2020',
         'GMT+3', '+02:00', 'monday ago', '+3 weekdays ago', ''];
foreach ($rows as $s) {
    printf("%-30s %s\n", json_encode($s), json_encode(date_parse($s)));
}
?>
--EXPECT--
"2020-01-02"                   {"year":2020,"month":1,"day":2,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"2020-01-02 12:00:00.5 +02:00" {"year":2020,"month":1,"day":2,"hour":12,"minute":0,"second":0,"fraction":0.5,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":7200,"is_dst":false}
"2020-01-02 12:00 UTC"         {"year":2020,"month":1,"day":2,"hour":12,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":3,"tz_abbr":"UTC","tz_id":"UTC"}
"xyz"                          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["The timezone could not be found in the database"],"is_localtime":true,"zone_type":0}
"xyz abc"                      {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":1,"warnings":{"4":"Double timezone specification"},"error_count":1,"errors":["The timezone could not be found in the database"],"is_localtime":true,"zone_type":0}
"Z"                            {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":2,"zone":0,"is_dst":false,"tz_abbr":"Z"}
"2020-13-45 25:70:80"          {"year":2020,"month":1,"day":1,"hour":5,"minute":7,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":4,"errors":{"6":"Unexpected character","11":"Unexpected character","15":"Double time specification","18":"Unexpected character"},"is_localtime":true,"zone_type":1,"zone":-162000,"is_dst":false}
"2020-02-31"                   {"year":2020,"month":2,"day":31,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":1,"warnings":{"11":"The parsed date was invalid"},"error_count":0,"errors":[],"is_localtime":false}
"2020-102"                     {"year":2020,"month":1,"day":102,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":1,"warnings":{"9":"The parsed date was invalid"},"error_count":0,"errors":[],"is_localtime":false}
"now"                          {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"today"                        {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"noon"                         {"year":false,"month":false,"day":false,"hour":12,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"+1 day"                       {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":1,"hour":0,"minute":0,"second":0}}
"+0 day"                       {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0}}
"tomorrow"                     {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":1,"hour":0,"minute":0,"second":0}}
"yesterday"                    {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":-1,"hour":0,"minute":0,"second":0}}
"next monday"                  {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":1}}
"monday"                       {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":1}}
"this week"                    {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":1}}
"next week"                    {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":7,"hour":0,"minute":0,"second":0,"weekday":1}}
"+3 weekdays"                  {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekdays":3}}
"first day of"                 {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"first_day_of_month":true}}
"last day of"                  {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"last_day_of_month":true}}
"first monday of january"      {"year":false,"month":1,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":1,"warnings":{"24":"The parsed date was invalid"},"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":1}}
"@100"                         {"year":1970,"month":1,"day":1,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":100}}
"@0"                           {"year":1970,"month":1,"day":1,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0}}
"@-1.5"                        {"year":1970,"month":1,"day":1,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":-1}}
"2020-W05"                     {"year":2020,"month":1,"day":1,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":26,"hour":0,"minute":0,"second":0}}
"2020-W05-3"                   {"year":2020,"month":1,"day":1,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":28,"hour":0,"minute":0,"second":0}}
"12:00"                        {"year":false,"month":false,"day":false,"hour":12,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"3pm"                          {"year":false,"month":false,"day":false,"hour":15,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"1234"                         {"year":false,"month":false,"day":false,"hour":12,"minute":34,"second":0,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"123456"                       {"year":false,"month":false,"day":false,"hour":12,"minute":34,"second":56,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"t9"                           {"year":false,"month":false,"day":false,"hour":9,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"2020"                         {"year":false,"month":false,"day":false,"hour":20,"minute":20,"second":0,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"2500"                         {"year":2500,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"20240102"                     {"year":2024,"month":1,"day":2,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"jan"                          {"year":false,"month":1,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":1,"warnings":{"4":"The parsed date was invalid"},"error_count":0,"errors":[],"is_localtime":false}
"january"                      {"year":false,"month":1,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":1,"warnings":{"8":"The parsed date was invalid"},"error_count":0,"errors":[],"is_localtime":false}
"january 2020"                 {"year":2020,"month":1,"day":1,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"january 12"                   {"year":false,"month":1,"day":12,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"@100 january 12"              {"year":false,"month":1,"day":12,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":100}}
"@100 12 january"              {"year":1970,"month":1,"day":12,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":0,"is_dst":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":100}}
"4\/20"                        {"year":false,"month":4,"day":20,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"1.2.2020"                     {"year":2020,"month":2,"day":1,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false}
"GMT+3"                        {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":10800,"is_dst":false}
"+02:00"                       {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":true,"zone_type":1,"zone":7200,"is_dst":false}
"monday ago"                   {"year":false,"month":false,"day":false,"hour":0,"minute":0,"second":0,"fraction":0,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekday":-1}}
"+3 weekdays ago"              {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":0,"errors":[],"is_localtime":false,"relative":{"year":0,"month":0,"day":0,"hour":0,"minute":0,"second":0,"weekdays":-3}}
""                             {"year":false,"month":false,"day":false,"hour":false,"minute":false,"second":false,"fraction":false,"warning_count":0,"warnings":[],"error_count":1,"errors":["Empty string"],"is_localtime":false}
