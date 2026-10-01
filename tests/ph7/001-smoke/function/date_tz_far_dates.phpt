--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Past the last listed transition, and before the first
--FILE--
<?php
/* A TZif file stops listing transitions in 2037 and ends with a POSIX rule
 * string -- `EST5EDT,M3.2.0,M11.1.0` -- that covers every year after it. A
 * reader that stops at the last listed transition answers standard time for
 * every date past 2037 and looks right in any test that does not cross one.
 *
 * The other end is the file's first ttinfo, which is where a zone's LMT lives:
 * New York in 1800 is -04:56, not -05:00. */
date_default_timezone_set('UTC');
$f = 'Y-m-d H:i:s P T I';

foreach (['America/New_York', 'Europe/Paris', 'Australia/Sydney', 'Asia/Tokyo',
          'America/Sao_Paulo'] as $zone) {
    $z = new DateTimeZone($zone);
    foreach (['1800-06-15 12:00:00', '1900-06-15 12:00:00', '2038-01-15 12:00:00',
              '2038-07-15 12:00:00', '2100-01-15 12:00:00', '2100-07-15 12:00:00',
              '2400-07-15 12:00:00'] as $s) {
        printf("%-18s %s %s\n", $zone, $s, (new DateTime($s, $z))->format($f));
    }
}
?>
--EXPECT--
America/New_York   1800-06-15 12:00:00 1800-06-15 12:00:00 -04:56 LMT 0
America/New_York   1900-06-15 12:00:00 1900-06-15 12:00:00 -05:00 EST 0
America/New_York   2038-01-15 12:00:00 2038-01-15 12:00:00 -05:00 EST 0
America/New_York   2038-07-15 12:00:00 2038-07-15 12:00:00 -04:00 EDT 1
America/New_York   2100-01-15 12:00:00 2100-01-15 12:00:00 -05:00 EST 0
America/New_York   2100-07-15 12:00:00 2100-07-15 12:00:00 -04:00 EDT 1
America/New_York   2400-07-15 12:00:00 2400-07-15 12:00:00 -04:00 EDT 1
Europe/Paris       1800-06-15 12:00:00 1800-06-15 12:00:00 +00:09 LMT 0
Europe/Paris       1900-06-15 12:00:00 1900-06-15 12:00:00 +00:09 PMT 0
Europe/Paris       2038-01-15 12:00:00 2038-01-15 12:00:00 +01:00 CET 0
Europe/Paris       2038-07-15 12:00:00 2038-07-15 12:00:00 +02:00 CEST 1
Europe/Paris       2100-01-15 12:00:00 2100-01-15 12:00:00 +01:00 CET 0
Europe/Paris       2100-07-15 12:00:00 2100-07-15 12:00:00 +02:00 CEST 1
Europe/Paris       2400-07-15 12:00:00 2400-07-15 12:00:00 +02:00 CEST 1
Australia/Sydney   1800-06-15 12:00:00 1800-06-15 12:00:00 +10:04 LMT 0
Australia/Sydney   1900-06-15 12:00:00 1900-06-15 12:00:00 +10:00 AEST 0
Australia/Sydney   2038-01-15 12:00:00 2038-01-15 12:00:00 +11:00 AEDT 1
Australia/Sydney   2038-07-15 12:00:00 2038-07-15 12:00:00 +10:00 AEST 0
Australia/Sydney   2100-01-15 12:00:00 2100-01-15 12:00:00 +11:00 AEDT 1
Australia/Sydney   2100-07-15 12:00:00 2100-07-15 12:00:00 +10:00 AEST 0
Australia/Sydney   2400-07-15 12:00:00 2400-07-15 12:00:00 +10:00 AEST 0
Asia/Tokyo         1800-06-15 12:00:00 1800-06-15 12:00:00 +09:18 LMT 0
Asia/Tokyo         1900-06-15 12:00:00 1900-06-15 12:00:00 +09:00 JST 0
Asia/Tokyo         2038-01-15 12:00:00 2038-01-15 12:00:00 +09:00 JST 0
Asia/Tokyo         2038-07-15 12:00:00 2038-07-15 12:00:00 +09:00 JST 0
Asia/Tokyo         2100-01-15 12:00:00 2100-01-15 12:00:00 +09:00 JST 0
Asia/Tokyo         2100-07-15 12:00:00 2100-07-15 12:00:00 +09:00 JST 0
Asia/Tokyo         2400-07-15 12:00:00 2400-07-15 12:00:00 +09:00 JST 0
America/Sao_Paulo  1800-06-15 12:00:00 1800-06-15 12:00:00 -03:06 LMT 0
America/Sao_Paulo  1900-06-15 12:00:00 1900-06-15 12:00:00 -03:06 LMT 0
America/Sao_Paulo  2038-01-15 12:00:00 2038-01-15 12:00:00 -03:00 -03 0
America/Sao_Paulo  2038-07-15 12:00:00 2038-07-15 12:00:00 -03:00 -03 0
America/Sao_Paulo  2100-01-15 12:00:00 2100-01-15 12:00:00 -03:00 -03 0
America/Sao_Paulo  2100-07-15 12:00:00 2100-07-15 12:00:00 -03:00 -03 0
America/Sao_Paulo  2400-07-15 12:00:00 2400-07-15 12:00:00 -03:00 -03 0
