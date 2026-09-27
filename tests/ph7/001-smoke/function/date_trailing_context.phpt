--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the byte php's meridian and its month-and-day rule demand behind them
--FILE--
<?php
/* Two of php's rules spell a TRAILING byte into the pattern, so what follows
 * decides whether the rule matched at all.
 *
 * The MERIDIAN ends in `[\0\t ]`, and php's buffer is NUL-padded so the end of
 * the string satisfies it too. Nothing else does: `3pm,`, `3pm\nx` and `3pm.x`
 * are no meridian, and the `am` of `11:30am\nx` is read as a zone instead. PHL
 * took a newline and a carriage return there and refused the NUL.
 *
 * The month-and-day rule ends in a RUN of `[,.stndrh\t ]`, which is what its
 * ordinal suffix and the comma before a year really are -- greedy, and REQUIRED
 * when no year follows. `january 12x` is therefore no such date, and neither is
 * `january 32`, whose day is out of the pattern's own range; but the month NAME
 * is still a token of its own, so php reads January and leaves the digits to
 * the string. Only the month-FIRST spelling falls back that way -- `87 january`
 * is php's refusal at position 0 -- and the run's greed is visible: it eats the
 * ` s` of `january 12 sat` and leaves `at` to be read as a zone. */
date_default_timezone_set('UTC');
$rows = ["3pm", "3pm ", "3pm\t", "3pm\0", "3pm\n", "3pm,", "3pm.", "3pm.x", "3pmx",
         "3pm\nx", "3pm\0x", "3pm\rx", "11:30am", "11:30am\nx", "11:30am\0x",
         "january 12", "january 12x", "january 12sd", "january 12sd2020",
         "january 12 seconds", "january 12 sat", "january 12 rrrx", "january 15, 2020",
         "january, 15 2020", "january 12th 2020", "january 12 2020", "january 32",
         "january 0x", "january 12345", "january  12/31", "jan 12x", "jan 1s",
         "87 january", "15 januaryx", "15th january", "january"];
$base = '2019-06-15 08:09:10';
foreach ($rows as $s) {
    try { $d = new DateTime($base); $d->modify($s); $r = $d->format('Y-m-d H:i:s'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-22s %s\n", json_encode($s), str_replace(["\n","\r","\0"], '?', $r));
}
?>
--EXPECT--
"3pm"                  2019-06-15 15:00:00
"3pm "                 2019-06-15 15:00:00
"3pm\t"                2019-06-15 15:00:00
"3pm\u0000"            2019-06-15 15:00:00
"3pm\n"                2019-06-15 15:00:00
"3pm,"                 DateTime::modify(): Failed to parse time string (3pm,) at position 0 (3): Unexpected character
"3pm."                 2019-06-15 15:00:00
"3pm.x"                DateTime::modify(): Failed to parse time string (3pm.x) at position 0 (3): Unexpected character
"3pmx"                 DateTime::modify(): Failed to parse time string (3pmx) at position 0 (3): Unexpected character
"3pm\nx"               DateTime::modify(): Failed to parse time string (3pm?x) at position 0 (3): Unexpected character
"3pm\u0000x"           2019-06-15 15:00:00
"3pm\rx"               DateTime::modify(): Failed to parse time string (3pm?x) at position 0 (3): Unexpected character
"11:30am"              2019-06-15 11:30:00
"11:30am\nx"           DateTime::modify(): Failed to parse time string (11:30am?x) at position 5 (a): The timezone could not be found in the database
"11:30am\u0000x"       2019-06-15 11:30:00
"january 12"           2019-01-12 08:09:10
"january 12x"          DateTime::modify(): Failed to parse time string (january 12x) at position 8 (1): Unexpected character
"january 12sd"         2019-01-12 08:09:10
"january 12sd2020"     2020-01-12 08:09:10
"january 12 seconds"   DateTime::modify(): Failed to parse time string (january 12 seconds) at position 12 (e): The timezone could not be found in the database
"january 12 sat"       DateTime::modify(): Failed to parse time string (january 12 sat) at position 12 (a): The timezone could not be found in the database
"january 12 rrrx"      2019-01-12 08:09:10
"january 15, 2020"     2020-01-15 08:09:10
"january, 15 2020"     DateTime::modify(): Failed to parse time string (january, 15 2020) at position 9 (1): Unexpected character
"january 12th 2020"    2020-01-12 08:09:10
"january 12 2020"      2020-01-12 08:09:10
"january 32"           DateTime::modify(): Failed to parse time string (january 32) at position 8 (3): Unexpected character
"january 0x"           DateTime::modify(): Failed to parse time string (january 0x) at position 8 (0): Unexpected character
"january 12345"        DateTime::modify(): Failed to parse time string (january 12345) at position 12 (5): Unexpected character
"january  12\/31"      DateTime::modify(): Failed to parse time string (january  12/31) at position 9 (1): Double date specification
"jan 12x"              DateTime::modify(): Failed to parse time string (jan 12x) at position 4 (1): Unexpected character
"jan 1s"               2019-01-01 08:09:10
"87 january"           DateTime::modify(): Failed to parse time string (87 january) at position 0 (8): Unexpected character
"15 januaryx"          2019-01-15 08:09:10
"15th january"         2019-01-15 08:09:10
"january"              2019-01-15 08:09:10
