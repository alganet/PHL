--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
what php COLLECTS from a date string, not what it stops on
--FILE--
<?php
/* php's scanner records a diagnostic and READS ON, so `getLastErrors()` answers
 * every refusal a string met -- `2020-13-45 25:70:80` is four of them -- where
 * PHL published the one it stopped at. The walk resumes one byte past what was
 * named, which is where php's catch-all rule leaves its cursor; a token that
 * MATCHED and whose action then complained is already behind the cursor.
 *
 * With them, three WARNINGS this engine did not raise at all. A second timezone
 * token is one (the third is the refusal), and the other two are asked once the
 * scan is over, at the byte one past the string: a date nobody could hold and a
 * clock nobody could show are warnings on a parse that otherwise SUCCEEDS. They
 * ride php's have_date / have_time rather than the fields, which is why
 * `january` warns -- a month with no day is not a date anyone can hold -- and
 * `2500`, whose bare year4 is no date at all, does not.
 *
 * Only the constructor and modify() publish the record in php; a clean parse
 * puts it back to `false`. */
date_default_timezone_set('UTC');
$rows = ['2020-13-45 25:70:80', '2020-13-45', '2020-01-32', '!!!!!!!!!!', 'xyz abc',
         '2020-01-02 UTC GMT', '+01:00 +02:00 +03:00', '12:00 13:00', '3pm 4pm',
         '2020-02-31', '2020-01-02 24:00:00', '2020-02-31 24:00:00', '23:59:60',
         '0000-00-00', '2020-102', 'january', '2500', '2020-01-02', 'now',
         '20240102 . 1.2.2020 2020-13-45', '2020-01-02 24:00:00this week',
         'january 12 12:00', 'january 12 12', 'january 124', 'january 12 4',
         'jan 1.2.20', '2020.102 12:00t9'];
foreach ($rows as $s) {
    try { new DateTime($s); } catch (Throwable $e) { }
    printf("%-32s %s\n", $s, json_encode(DateTime::getLastErrors()));
}
?>
--EXPECT--
2020-13-45 25:70:80              {"warning_count":0,"warnings":[],"error_count":4,"errors":{"6":"Unexpected character","11":"Unexpected character","15":"Double time specification","18":"Unexpected character"}}
2020-13-45                       {"warning_count":0,"warnings":[],"error_count":1,"errors":{"6":"Unexpected character"}}
2020-01-32                       {"warning_count":0,"warnings":[],"error_count":1,"errors":{"9":"Unexpected character"}}
!!!!!!!!!!                       {"warning_count":0,"warnings":[],"error_count":10,"errors":["Unexpected character","Unexpected character","Unexpected character","Unexpected character","Unexpected character","Unexpected character","Unexpected character","Unexpected character","Unexpected character","Unexpected character"]}
xyz abc                          {"warning_count":1,"warnings":{"4":"Double timezone specification"},"error_count":1,"errors":["The timezone could not be found in the database"]}
2020-01-02 UTC GMT               {"warning_count":1,"warnings":{"15":"Double timezone specification"},"error_count":0,"errors":[]}
+01:00 +02:00 +03:00             {"warning_count":1,"warnings":{"7":"Double timezone specification"},"error_count":1,"errors":{"14":"Double timezone specification"}}
12:00 13:00                      {"warning_count":0,"warnings":[],"error_count":1,"errors":{"6":"Double time specification"}}
3pm 4pm                          {"warning_count":0,"warnings":[],"error_count":1,"errors":{"4":"Double time specification"}}
2020-02-31                       {"warning_count":1,"warnings":{"11":"The parsed date was invalid"},"error_count":0,"errors":[]}
2020-01-02 24:00:00              {"warning_count":1,"warnings":{"20":"The parsed time was invalid"},"error_count":0,"errors":[]}
2020-02-31 24:00:00              {"warning_count":2,"warnings":{"20":"The parsed date was invalid"},"error_count":0,"errors":[]}
23:59:60                         {"warning_count":1,"warnings":{"9":"The parsed time was invalid"},"error_count":0,"errors":[]}
0000-00-00                       {"warning_count":1,"warnings":{"11":"The parsed date was invalid"},"error_count":0,"errors":[]}
2020-102                         {"warning_count":1,"warnings":{"9":"The parsed date was invalid"},"error_count":0,"errors":[]}
january                          {"warning_count":1,"warnings":{"8":"The parsed date was invalid"},"error_count":0,"errors":[]}
2500                             false
2020-01-02                       false
now                              false
20240102 . 1.2.2020 2020-13-45   {"warning_count":0,"warnings":[],"error_count":3,"errors":{"11":"Double date specification","20":"Double date specification","26":"Unexpected character"}}
2020-01-02 24:00:00this week     {"warning_count":1,"warnings":{"29":"The parsed time was invalid"},"error_count":0,"errors":[]}
january 12 12:00                 false
january 12 12                    false
january 124                      {"warning_count":1,"warnings":{"12":"The parsed date was invalid"},"error_count":3,"errors":{"8":"Unexpected character","9":"Unexpected character","10":"Unexpected character"}}
january 12 4                     false
jan 1.2.20                       false
2020.102 12:00t9                 {"warning_count":1,"warnings":{"17":"The parsed date was invalid"},"error_count":1,"errors":{"14":"Double time specification"}}
