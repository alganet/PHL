--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php's separator bytes between date-string tokens
--FILE--
<?php
/* php's run between two tokens is wider than a space, and PHL skipped three
 * bytes of it: a date string that kept the NEWLINE of the file it was read
 * from -- `strtotime(fgets($fp))` -- did not parse at all, and neither did the
 * full stop php reads as a separator (`12:00.UTC`, `+1 day.+2 hours`).
 *
 * Between tokens the set is NUL, tab, newline, space, comma and the full stop.
 * At the two ENDS it is wider still -- a carriage return, a vertical tab and a
 * form feed close the string where the same bytes between two tokens are an
 * unexpected character -- while the comma and the full stop go the other way:
 * they separate tokens but do not close the string, which is what keeps
 * `12:00,` a time and `3pm,` no meridian at all. */
date_default_timezone_set('UTC');
$b = '2020-06-15 08:09:10';
$rows = ["12:00\n","12:00\r","12:00\v","12:00\f","12:00\0","12:00 ","12:00,","12:00.",
         "\n12:00","\r12:00","\v12:00","\f12:00"," \r\n\t12:00","12:00 \r\n",
         "12:00\nUTC","12:00\0UTC","12:00\vUTC","12:00\rUTC","12:00\fUTC","12:00.UTC","12:00,UTC",
         "2020-01-01.12:00","2020-01-01,12:00","2020-01-01\n12:00","2020-01-01\v12:00",
         ".12:00","12:00..","12:00 .","+1 day.+2 hours","1.2.2020","12:00.5","p.m","12:00.p",
         "3pm,","3pm ,","3pm.","3pm\n"];
foreach ($rows as $s) {
    try { $d = new DateTime($b); $d->modify($s); $r = $d->format('Y-m-d H:i:s.u P'); }
    catch (Throwable $e) { $r = $e->getMessage(); }
    printf("%-24s %s\n", json_encode($s), str_replace(["\n","\r","\v","\f","\0"], '?', $r));
}
?>
--EXPECT--
"12:00\n"                2020-06-15 12:00:00.000000 +00:00
"12:00\r"                2020-06-15 12:00:00.000000 +00:00
"12:00\u000b"            2020-06-15 12:00:00.000000 +00:00
"12:00\f"                2020-06-15 12:00:00.000000 +00:00
"12:00\u0000"            2020-06-15 12:00:00.000000 +00:00
"12:00 "                 2020-06-15 12:00:00.000000 +00:00
"12:00,"                 2020-06-15 12:00:00.000000 +00:00
"12:00."                 2020-06-15 12:00:00.000000 +00:00
"\n12:00"                2020-06-15 12:00:00.000000 +00:00
"\r12:00"                2020-06-15 12:00:00.000000 +00:00
"\u000b12:00"            2020-06-15 12:00:00.000000 +00:00
"\f12:00"                2020-06-15 12:00:00.000000 +00:00
" \r\n\t12:00"           2020-06-15 12:00:00.000000 +00:00
"12:00 \r\n"             2020-06-15 12:00:00.000000 +00:00
"12:00\nUTC"             2020-06-15 12:00:00.000000 +00:00
"12:00\u0000UTC"         2020-06-15 12:00:00.000000 +00:00
"12:00\u000bUTC"         DateTime::modify(): Failed to parse time string (12:00?UTC) at position 5 (?): Unexpected character
"12:00\rUTC"             DateTime::modify(): Failed to parse time string (12:00?UTC) at position 5 (?): Unexpected character
"12:00\fUTC"             DateTime::modify(): Failed to parse time string (12:00?UTC) at position 5 (?): Unexpected character
"12:00.UTC"              2020-06-15 12:00:00.000000 +00:00
"12:00,UTC"              2020-06-15 12:00:00.000000 +00:00
"2020-01-01.12:00"       2020-01-01 12:00:00.000000 +00:00
"2020-01-01,12:00"       2020-01-01 12:00:00.000000 +00:00
"2020-01-01\n12:00"      2020-01-01 12:00:00.000000 +00:00
"2020-01-01\u000b12:00"  DateTime::modify(): Failed to parse time string (2020-01-01?12:00) at position 10 (?): Unexpected character
".12:00"                 2020-06-15 12:00:00.000000 +00:00
"12:00.."                2020-06-15 12:00:00.000000 +00:00
"12:00 ."                2020-06-15 12:00:00.000000 +00:00
"+1 day.+2 hours"        2020-06-16 10:09:10.000000 +00:00
"1.2.2020"               2020-02-01 08:09:10.000000 +00:00
"12:00.5"                2020-06-15 12:00:05.000000 +00:00
"p.m"                    2020-06-15 08:09:10.000000 +00:00
"12:00.p"                2020-06-15 12:00:00.000000 +00:00
"3pm,"                   DateTime::modify(): Failed to parse time string (3pm,) at position 0 (3): Unexpected character
"3pm ,"                  2020-06-15 15:00:00.000000 +00:00
"3pm."                   2020-06-15 15:00:00.000000 +00:00
"3pm\n"                  2020-06-15 15:00:00.000000 +00:00
