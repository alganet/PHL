--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's twelve-hour clock: 3pm, 3:04 p.m., 12am
--FILE--
<?php
/* The twelve-hour clock did not parse at all -- `3pm`, `11:30am`, `3:04 p.m.`
 * and every other spelling of it was `Unexpected character` or an unknown
 * timezone -- so the way most people write a time of day did not run.
 *
 * php's meridian is `am`/`pm` in any case with a dot after either letter, and
 * nothing but whitespace or the end of the string behind it: `3pm.` is a time,
 * `3pm..`, `3pm,` and `3pmx` are not. The hour has to be one a twelve-hour
 * clock can name, so `0am` and `13pm` are refusals rather than times, and
 * `13:00pm` keeps its 13 with the `pm` left to the string -- which then reads
 * it as an unknown zone. The LAST field carries both its digits (`3:04pm` and
 * `3:4:05pm` are times, `3:4pm` and `3:04:5pm` are not), and a fraction
 * narrows the shape to php's one spelling of it: colon separators, both digits
 * on both fields, and the meridian immediately behind the fraction. The `t`
 * prefix belongs to the ISO spelling alone. */
date_default_timezone_set('UTC');
$b = '2020-06-15 08:09:10';
$rows = ["3pm","3 pm","3PM","3 P.M.","3p.m.","3a.m.","3am.","3a.m","12am","12pm","1pm","09pm","9 pm",
         "0am","0pm","00am","13pm","010pm","3p","3 a. m.","3pmx","3pm,","3pm.","3pm..","3pmUTC",
         "3:04pm","3:04:05pm","03:04:05 pm","3:04 p.m.","3.04pm","3.04.05pm","3.4:05pm","12:30am",
         "3:4pm","3.4pm","03:4pm","3:04:5pm","13:00pm","00:30am","23:00 pm","0:30am",
         "3:04:05.5pm","3:04:05.500000pm","3:04:05.5 pm","3:04:05.55pm","3:4:05.5pm","3:04.05.5pm",
         "3pm UTC","3pm +02:00","3:04am UTC","3 pm 2020-01-01","2020-01-01 3pm","3am ago",
         "t3pm","T3pm","x3pm","3pm 4pm","12:00","3 days"];
foreach ($rows as $s) {
    try { $d = new DateTime($b); $d->modify($s); $r = $d->format('Y-m-d H:i:s.u'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-20s %s\n", $s, $r);
}
?>
--EXPECT--
3pm                  2020-06-15 15:00:00.000000
3 pm                 2020-06-15 15:00:00.000000
3PM                  2020-06-15 15:00:00.000000
3 P.M.               2020-06-15 15:00:00.000000
3p.m.                2020-06-15 15:00:00.000000
3a.m.                2020-06-15 03:00:00.000000
3am.                 2020-06-15 03:00:00.000000
3a.m                 2020-06-15 03:00:00.000000
12am                 2020-06-15 00:00:00.000000
12pm                 2020-06-15 12:00:00.000000
1pm                  2020-06-15 13:00:00.000000
09pm                 2020-06-15 21:00:00.000000
9 pm                 2020-06-15 21:00:00.000000
0am                  DateTime::modify(): Failed to parse time string (0am) at position 0 (0): Unexpected character
0pm                  DateTime::modify(): Failed to parse time string (0pm) at position 0 (0): Unexpected character
00am                 DateTime::modify(): Failed to parse time string (00am) at position 0 (0): Unexpected character
13pm                 DateTime::modify(): Failed to parse time string (13pm) at position 0 (1): Unexpected character
010pm                DateTime::modify(): Failed to parse time string (010pm) at position 0 (0): Unexpected character
3p                   DateTime::modify(): Failed to parse time string (3p) at position 0 (3): Unexpected character
3 a. m.              DateTime::modify(): Failed to parse time string (3 a. m.) at position 0 (3): Unexpected character
3pmx                 DateTime::modify(): Failed to parse time string (3pmx) at position 0 (3): Unexpected character
3pm,                 DateTime::modify(): Failed to parse time string (3pm,) at position 0 (3): Unexpected character
3pm.                 2020-06-15 15:00:00.000000
3pm..                DateTime::modify(): Failed to parse time string (3pm..) at position 0 (3): Unexpected character
3pmUTC               DateTime::modify(): Failed to parse time string (3pmUTC) at position 0 (3): Unexpected character
3:04pm               2020-06-15 15:04:00.000000
3:04:05pm            2020-06-15 15:04:05.000000
03:04:05 pm          2020-06-15 15:04:05.000000
3:04 p.m.            2020-06-15 15:04:00.000000
3.04pm               2020-06-15 15:04:00.000000
3.04.05pm            2020-06-15 15:04:05.000000
3.4:05pm             2020-06-15 15:04:05.000000
12:30am              2020-06-15 00:30:00.000000
3:4pm                DateTime::modify(): Failed to parse time string (3:4pm) at position 3 (p): The timezone could not be found in the database
3.4pm                DateTime::modify(): Failed to parse time string (3.4pm) at position 3 (p): The timezone could not be found in the database
03:4pm               DateTime::modify(): Failed to parse time string (03:4pm) at position 4 (p): The timezone could not be found in the database
3:04:5pm             DateTime::modify(): Failed to parse time string (3:04:5pm) at position 6 (p): The timezone could not be found in the database
13:00pm              DateTime::modify(): Failed to parse time string (13:00pm) at position 5 (p): The timezone could not be found in the database
00:30am              DateTime::modify(): Failed to parse time string (00:30am) at position 5 (a): The timezone could not be found in the database
23:00 pm             DateTime::modify(): Failed to parse time string (23:00 pm) at position 6 (p): The timezone could not be found in the database
0:30am               DateTime::modify(): Failed to parse time string (0:30am) at position 4 (a): The timezone could not be found in the database
3:04:05.5pm          2020-06-15 15:04:05.500000
3:04:05.500000pm     2020-06-15 15:04:05.500000
3:04:05.5 pm         DateTime::modify(): Failed to parse time string (3:04:05.5 pm) at position 10 (p): The timezone could not be found in the database
3:04:05.55pm         2020-06-15 15:04:05.550000
3:4:05.5pm           DateTime::modify(): Failed to parse time string (3:4:05.5pm) at position 8 (p): The timezone could not be found in the database
3:04.05.5pm          DateTime::modify(): Failed to parse time string (3:04.05.5pm) at position 9 (p): The timezone could not be found in the database
3pm UTC              2020-06-15 15:00:00.000000
3pm +02:00           2020-06-15 15:00:00.000000
3:04am UTC           2020-06-15 03:04:00.000000
3 pm 2020-01-01      2020-01-01 15:00:00.000000
2020-01-01 3pm       2020-01-01 15:00:00.000000
3am ago              2020-06-15 03:00:00.000000
t3pm                 DateTime::modify(): Failed to parse time string (t3pm) at position 2 (p): The timezone could not be found in the database
T3pm                 DateTime::modify(): Failed to parse time string (T3pm) at position 2 (p): The timezone could not be found in the database
x3pm                 2020-06-15 15:00:00.000000
3pm 4pm              DateTime::modify(): Failed to parse time string (3pm 4pm) at position 4 (4): Double time specification
12:00                2020-06-15 12:00:00.000000
3 days               2020-06-18 08:09:10.000000
