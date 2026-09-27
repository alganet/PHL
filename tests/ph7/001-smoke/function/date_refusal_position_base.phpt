--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
what a date-string refusal reports: php's trimmed position, and the C-string cut
--FILE--
<?php
/* php TRIMS the string before its scanner ever runs -- isspace() off both ends,
 * timelib_strtotime's own first act -- so every position it reports afterwards
 * is the TRIMMED string's while the message still prints what the caller wrote.
 * PHL measured from the original, so `new DateTime("  xyz")` blamed position 2
 * where php blames 0. The trim is isspace and NOTHING else: a leading NUL or
 * full stop is an ordinary separator the scanner steps OVER, and a byte stepped
 * over still counts.
 *
 * And what the sentence SHOWS of the string stops at the first NUL, because php
 * hands it to a C `%s` -- a date string may carry one, since the scanner reads a
 * NUL as a separator, so the byte php names can sit past what php prints. */
date_default_timezone_set('UTC');
$strings = ["xyz", "  xyz", "\txyz", "\nxyz", "\r\n\v\fxyz", "xyz  ", "  xyz  ",
            ".xyz", "..xyz", " .xyz", ".. xyz", "\0xyz", "  --", "  2020-13-45",
            "15 january 2020\0),/", "  15 january 2020\0),/"];
foreach ($strings as $s) {
    foreach (['ctor', 'modify', 'fromDateString'] as $verb) {
        try {
            switch ($verb) {
                case 'ctor':   new DateTime($s); break;
                case 'modify': (new DateTime('@0'))->modify($s); break;
                default:       DateInterval::createFromDateString($s); break;
            }
            $r = 'OK';
        } catch (Throwable $e) {
            $r = get_class($e) . ': ' . $e->getMessage();
        }
        printf("%-26s %-14s %s\n", json_encode($s), $verb, str_replace(["\n","\r","\v","\f","\0"], '?', $r));
    }
}
?>
--EXPECT--
"xyz"                      ctor           DateMalformedStringException: Failed to parse time string (xyz) at position 0 (x): The timezone could not be found in the database
"xyz"                      modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (xyz) at position 0 (x): The timezone could not be found in the database
"xyz"                      fromDateString DateMalformedIntervalStringException: Unknown or bad format (xyz) at position 0 (x): The timezone could not be found in the database
"  xyz"                    ctor           DateMalformedStringException: Failed to parse time string (  xyz) at position 0 (x): The timezone could not be found in the database
"  xyz"                    modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (  xyz) at position 0 (x): The timezone could not be found in the database
"  xyz"                    fromDateString DateMalformedIntervalStringException: Unknown or bad format (  xyz) at position 0 (x): The timezone could not be found in the database
"\txyz"                    ctor           DateMalformedStringException: Failed to parse time string (	xyz) at position 0 (x): The timezone could not be found in the database
"\txyz"                    modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (	xyz) at position 0 (x): The timezone could not be found in the database
"\txyz"                    fromDateString DateMalformedIntervalStringException: Unknown or bad format (	xyz) at position 0 (x): The timezone could not be found in the database
"\nxyz"                    ctor           DateMalformedStringException: Failed to parse time string (?xyz) at position 0 (x): The timezone could not be found in the database
"\nxyz"                    modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (?xyz) at position 0 (x): The timezone could not be found in the database
"\nxyz"                    fromDateString DateMalformedIntervalStringException: Unknown or bad format (?xyz) at position 0 (x): The timezone could not be found in the database
"\r\n\u000b\fxyz"          ctor           DateMalformedStringException: Failed to parse time string (????xyz) at position 0 (x): The timezone could not be found in the database
"\r\n\u000b\fxyz"          modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (????xyz) at position 0 (x): The timezone could not be found in the database
"\r\n\u000b\fxyz"          fromDateString DateMalformedIntervalStringException: Unknown or bad format (????xyz) at position 0 (x): The timezone could not be found in the database
"xyz  "                    ctor           DateMalformedStringException: Failed to parse time string (xyz  ) at position 0 (x): The timezone could not be found in the database
"xyz  "                    modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (xyz  ) at position 0 (x): The timezone could not be found in the database
"xyz  "                    fromDateString DateMalformedIntervalStringException: Unknown or bad format (xyz  ) at position 0 (x): The timezone could not be found in the database
"  xyz  "                  ctor           DateMalformedStringException: Failed to parse time string (  xyz  ) at position 0 (x): The timezone could not be found in the database
"  xyz  "                  modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (  xyz  ) at position 0 (x): The timezone could not be found in the database
"  xyz  "                  fromDateString DateMalformedIntervalStringException: Unknown or bad format (  xyz  ) at position 0 (x): The timezone could not be found in the database
".xyz"                     ctor           DateMalformedStringException: Failed to parse time string (.xyz) at position 1 (x): The timezone could not be found in the database
".xyz"                     modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (.xyz) at position 1 (x): The timezone could not be found in the database
".xyz"                     fromDateString DateMalformedIntervalStringException: Unknown or bad format (.xyz) at position 1 (x): The timezone could not be found in the database
"..xyz"                    ctor           DateMalformedStringException: Failed to parse time string (..xyz) at position 2 (x): The timezone could not be found in the database
"..xyz"                    modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (..xyz) at position 2 (x): The timezone could not be found in the database
"..xyz"                    fromDateString DateMalformedIntervalStringException: Unknown or bad format (..xyz) at position 2 (x): The timezone could not be found in the database
" .xyz"                    ctor           DateMalformedStringException: Failed to parse time string ( .xyz) at position 1 (x): The timezone could not be found in the database
" .xyz"                    modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string ( .xyz) at position 1 (x): The timezone could not be found in the database
" .xyz"                    fromDateString DateMalformedIntervalStringException: Unknown or bad format ( .xyz) at position 1 (x): The timezone could not be found in the database
".. xyz"                   ctor           DateMalformedStringException: Failed to parse time string (.. xyz) at position 3 (x): The timezone could not be found in the database
".. xyz"                   modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (.. xyz) at position 3 (x): The timezone could not be found in the database
".. xyz"                   fromDateString DateMalformedIntervalStringException: Unknown or bad format (.. xyz) at position 3 (x): The timezone could not be found in the database
"\u0000xyz"                ctor           DateMalformedStringException: Failed to parse time string () at position 1 (x): The timezone could not be found in the database
"\u0000xyz"                modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string () at position 1 (x): The timezone could not be found in the database
"\u0000xyz"                fromDateString DateMalformedIntervalStringException: Unknown or bad format () at position 1 (x): The timezone could not be found in the database
"  --"                     ctor           DateMalformedStringException: Failed to parse time string (  --) at position 0 (-): Unexpected character
"  --"                     modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (  --) at position 0 (-): Unexpected character
"  --"                     fromDateString DateMalformedIntervalStringException: Unknown or bad format (  --) at position 0 (-): Unexpected character
"  2020-13-45"             ctor           DateMalformedStringException: Failed to parse time string (  2020-13-45) at position 6 (3): Unexpected character
"  2020-13-45"             modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (  2020-13-45) at position 6 (3): Unexpected character
"  2020-13-45"             fromDateString DateMalformedIntervalStringException: Unknown or bad format (  2020-13-45) at position 6 (3): Unexpected character
"15 january 2020\u0000),\/" ctor           DateMalformedStringException: Failed to parse time string (15 january 2020) at position 16 ()): Unexpected character
"15 january 2020\u0000),\/" modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (15 january 2020) at position 16 ()): Unexpected character
"15 january 2020\u0000),\/" fromDateString DateMalformedIntervalStringException: Unknown or bad format (15 january 2020) at position 16 ()): Unexpected character
"  15 january 2020\u0000),\/" ctor           DateMalformedStringException: Failed to parse time string (  15 january 2020) at position 16 ()): Unexpected character
"  15 january 2020\u0000),\/" modify         DateMalformedStringException: DateTime::modify(): Failed to parse time string (  15 january 2020) at position 16 ()): Unexpected character
"  15 january 2020\u0000),\/" fromDateString DateMalformedIntervalStringException: Unknown or bad format (  15 january 2020) at position 16 ()): Unexpected character
