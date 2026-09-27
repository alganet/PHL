--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
modify() parses a zone in its string and then discards it
--FILE--
<?php
/* php's modify() copies the FIELDS its string parsed into the object and nothing
 * else, so a zone the modifier names moves nothing: `$d->modify('2020-01-01T12:00:00Z')`
 * on a +05:00 date is noon AT +05:00 there, not 17:00. PHL applied the parsed
 * offset to the clock, so every modifier carrying one shifted the receiver by its
 * own offset -- silently, and only when the two zones differed.
 *
 * `@epoch` is the exception php makes: that form names an absolute INSTANT, and
 * it re-zones the object to the fixed `+00:00` along with it. */
date_default_timezone_set('UTC');

function dmz_show($mod)
{
    foreach (['+05:00', 'UTC', '-03:00'] as $tz) {
        $d = new DateTime('2020-06-01 07:08:09', new DateTimeZone($tz));
        try {
            $d->modify($mod);
            printf("%-26s %-7s => %s %s\n", $mod, $tz, $d->format('Y-m-d H:i:s P'),
                $d->getTimezone()->getName());
        } catch (Throwable $e) {
            printf("%-26s %-7s => %s\n", $mod, $tz, $e->getMessage());
        }
    }
}

echo "--- a zone in the modifier moves nothing\n";
foreach (['2020-01-01T12:00:00+02:00', '2020-01-01T12:00:00Z', '1 January 2020 UTC',
          '12:00 UTC', 'UTC', '+0200', '2020-01-01 12:00 T'] as $mod) {
    dmz_show($mod);
}

echo "--- @epoch names an instant, and re-zones the object\n";
foreach (['@0', '@86400', '@0 +1 day'] as $mod) {
    dmz_show($mod);
}

echo "--- the modifiers that name no zone are unchanged\n";
foreach (['+1 day', 'yesterday', '2020-01-01', 'first day of next month'] as $mod) {
    dmz_show($mod);
}

echo "--- the procedural spelling and the immutable one agree\n";
$d = new DateTime('2020-06-01 07:08:09', new DateTimeZone('+05:00'));
date_modify($d, '2020-01-01T12:00:00Z');
echo $d->format('Y-m-d H:i:s P e'), "\n";
$i = new DateTimeImmutable('2020-06-01 07:08:09', new DateTimeZone('+05:00'));
echo $i->modify('2020-01-01T12:00:00Z')->format('Y-m-d H:i:s P e'), "\n";
echo $i->format('Y-m-d H:i:s P e'), "\n";

echo "--- a zone in a CONSTRUCTOR string still wins, and strtotime still reads it\n";
$c = new DateTime('2020-01-01T12:00:00Z', new DateTimeZone('+05:00'));
echo $c->format('Y-m-d H:i:s P e'), "\n";
var_dump(strtotime('2020-01-01 12:00:00 +0200'), strtotime('2020-01-01 12:00:00 UTC'));
?>
--EXPECT--
--- a zone in the modifier moves nothing
2020-01-01T12:00:00+02:00  +05:00  => 2020-01-01 12:00:00 +05:00 +05:00
2020-01-01T12:00:00+02:00  UTC     => 2020-01-01 12:00:00 +00:00 UTC
2020-01-01T12:00:00+02:00  -03:00  => 2020-01-01 12:00:00 -03:00 -03:00
2020-01-01T12:00:00Z       +05:00  => 2020-01-01 12:00:00 +05:00 +05:00
2020-01-01T12:00:00Z       UTC     => 2020-01-01 12:00:00 +00:00 UTC
2020-01-01T12:00:00Z       -03:00  => 2020-01-01 12:00:00 -03:00 -03:00
1 January 2020 UTC         +05:00  => 2020-01-01 07:08:09 +05:00 +05:00
1 January 2020 UTC         UTC     => 2020-01-01 07:08:09 +00:00 UTC
1 January 2020 UTC         -03:00  => 2020-01-01 07:08:09 -03:00 -03:00
12:00 UTC                  +05:00  => 2020-06-01 12:00:00 +05:00 +05:00
12:00 UTC                  UTC     => 2020-06-01 12:00:00 +00:00 UTC
12:00 UTC                  -03:00  => 2020-06-01 12:00:00 -03:00 -03:00
UTC                        +05:00  => 2020-06-01 07:08:09 +05:00 +05:00
UTC                        UTC     => 2020-06-01 07:08:09 +00:00 UTC
UTC                        -03:00  => 2020-06-01 07:08:09 -03:00 -03:00
+0200                      +05:00  => 2020-06-01 07:08:09 +05:00 +05:00
+0200                      UTC     => 2020-06-01 07:08:09 +00:00 UTC
+0200                      -03:00  => 2020-06-01 07:08:09 -03:00 -03:00
2020-01-01 12:00 T         +05:00  => 2020-01-01 12:00:00 +05:00 +05:00
2020-01-01 12:00 T         UTC     => 2020-01-01 12:00:00 +00:00 UTC
2020-01-01 12:00 T         -03:00  => 2020-01-01 12:00:00 -03:00 -03:00
--- @epoch names an instant, and re-zones the object
@0                         +05:00  => 1970-01-01 00:00:00 +00:00 +00:00
@0                         UTC     => 1970-01-01 00:00:00 +00:00 +00:00
@0                         -03:00  => 1970-01-01 00:00:00 +00:00 +00:00
@86400                     +05:00  => 1970-01-02 00:00:00 +00:00 +00:00
@86400                     UTC     => 1970-01-02 00:00:00 +00:00 +00:00
@86400                     -03:00  => 1970-01-02 00:00:00 +00:00 +00:00
@0 +1 day                  +05:00  => 1970-01-02 00:00:00 +00:00 +00:00
@0 +1 day                  UTC     => 1970-01-02 00:00:00 +00:00 +00:00
@0 +1 day                  -03:00  => 1970-01-02 00:00:00 +00:00 +00:00
--- the modifiers that name no zone are unchanged
+1 day                     +05:00  => 2020-06-02 07:08:09 +05:00 +05:00
+1 day                     UTC     => 2020-06-02 07:08:09 +00:00 UTC
+1 day                     -03:00  => 2020-06-02 07:08:09 -03:00 -03:00
yesterday                  +05:00  => 2020-05-31 00:00:00 +05:00 +05:00
yesterday                  UTC     => 2020-05-31 00:00:00 +00:00 UTC
yesterday                  -03:00  => 2020-05-31 00:00:00 -03:00 -03:00
2020-01-01                 +05:00  => 2020-01-01 07:08:09 +05:00 +05:00
2020-01-01                 UTC     => 2020-01-01 07:08:09 +00:00 UTC
2020-01-01                 -03:00  => 2020-01-01 07:08:09 -03:00 -03:00
first day of next month    +05:00  => 2020-07-01 07:08:09 +05:00 +05:00
first day of next month    UTC     => 2020-07-01 07:08:09 +00:00 UTC
first day of next month    -03:00  => 2020-07-01 07:08:09 -03:00 -03:00
--- the procedural spelling and the immutable one agree
2020-01-01 12:00:00 +05:00 +05:00
2020-01-01 12:00:00 +05:00 +05:00
2020-06-01 07:08:09 +05:00 +05:00
--- a zone in a CONSTRUCTOR string still wins, and strtotime still reads it
2020-01-01 12:00:00 +00:00 Z
int(1577872800)
int(1577880000)
