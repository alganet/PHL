--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A tz-database IDENTIFIER: case-folded lookup, verbatim name, GMT's two readings
--FILE--
<?php
/* §10's timezone-database cut is lifted behind PH7_ENABLE_TZDB, and this is the
 * shape of what came in: a DateTimeZone can name a place. Four things about it
 * are not obvious and each was measured against php rather than assumed.
 *
 *  - the LOOKUP folds case and the NAME does not: php resolves `europe/paris`
 *    and then stores the caller's own bytes, so getName() echoes the spelling
 *    back while format('e') on a date in it does too. An identifier is not
 *    canonicalised anywhere.
 *  - a leading blank is dropped and a trailing one is a refusal, which is the
 *    same rule the offset grammar beside it already had.
 *  - `GMT` is an offset PREFIX only when a sign follows it. `GMT+0` is the fixed
 *    offset `+00:00`; `GMT0` is a tz-database identifier stored verbatim; `GMT8`
 *    is neither and is refused.
 *  - the offset depends on the INSTANT, so getOffset() reads the date it is
 *    handed -- the whole point of a database zone.
 */
date_default_timezone_set('UTC');

foreach (['Europe/Paris', 'europe/paris', 'EUROPE/PARIS', ' Europe/Paris',
          'America/Argentina/Buenos_Aires', 'Etc/GMT+5', 'etc/gmt+5',
          'GMT0', 'gmt0', 'GMT+0', 'US/Eastern', 'W-SU', 'Factory'] as $name) {
    $z = new DateTimeZone($name);
    printf("%-32s name=%-32s type=%d\n", "[$name]", $z->getName(),
        ((array) $z)['timezone_type']);
}

foreach (['Europe/Paris ', 'GMT8', 'GMTfoo', 'Nowhere/Nothing', 'Europe/', '/'] as $name) {
    try {
        new DateTimeZone($name);
        echo "[$name] NOT REFUSED\n";
    } catch (Throwable $e) {
        printf("[%s] %s: %s\n", $name, get_class($e), $e->getMessage());
    }
}

$z = new DateTimeZone('America/New_York');
foreach ([0, 1268551800, 1278250000, 1289107800] as $ts) {
    printf("offset@%d = %d\n", $ts, $z->getOffset(new DateTime('@' . $ts)));
}
?>
--EXPECT--
[Europe/Paris]                   name=Europe/Paris                     type=3
[europe/paris]                   name=europe/paris                     type=3
[EUROPE/PARIS]                   name=EUROPE/PARIS                     type=3
[ Europe/Paris]                  name=Europe/Paris                     type=3
[America/Argentina/Buenos_Aires] name=America/Argentina/Buenos_Aires   type=3
[Etc/GMT+5]                      name=Etc/GMT+5                        type=3
[etc/gmt+5]                      name=etc/gmt+5                        type=3
[GMT0]                           name=GMT0                             type=3
[gmt0]                           name=gmt0                             type=3
[GMT+0]                          name=+00:00                           type=1
[US/Eastern]                     name=US/Eastern                       type=3
[W-SU]                           name=W-SU                             type=3
[Factory]                        name=Factory                          type=3
[Europe/Paris ] DateInvalidTimeZoneException: DateTimeZone::__construct(): Unknown or bad timezone (Europe/Paris )
[GMT8] DateInvalidTimeZoneException: DateTimeZone::__construct(): Unknown or bad timezone (GMT8)
[GMTfoo] DateInvalidTimeZoneException: DateTimeZone::__construct(): Unknown or bad timezone (GMTfoo)
[Nowhere/Nothing] DateInvalidTimeZoneException: DateTimeZone::__construct(): Unknown or bad timezone (Nowhere/Nothing)
[Europe/] DateInvalidTimeZoneException: DateTimeZone::__construct(): Unknown or bad timezone (Europe/)
[/] DateInvalidTimeZoneException: DateTimeZone::__construct(): Unknown or bad timezone (/)
offset@0 = -18000
offset@1268551800 = -14400
offset@1278250000 = -14400
offset@1289107800 = -14400
