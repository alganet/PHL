--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The date classes compare by what they MEAN, not by their properties
--FILE--
<?php
/* php gives the date classes compare HANDLERS, and the property-by-property
 * walk PHL fell back to disagreed with every one of them:
 *  - a DateTime and a DateTimeImmutable of the same INSTANT are equal there and
 *    were not here (two different classes never compared at all), and the zone
 *    a date is expressed in is no part of the comparison;
 *  - two DateIntervals are NEVER comparable -- an E_WARNING and php's
 *    uncomparable answer from either side, with only `$i == $i` true -- where
 *    PHL compared them field by field and called two `P1D`s equal;
 *  - two DateTimeZones of the same KIND answer 0 or "uncomparable" and never an
 *    ORDERING, so `+01:00 < +02:00` is false;
 *  - and any two DatePeriods are equal, because php fabricates the seven
 *    properties it shows and its real property table is empty. */
date_default_timezone_set('UTC');

function dtcmp_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-38s %s\n", $label, var_export($r, true));
    } catch (Throwable $e) {
        printf("%-38s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}

/* --- the INSTANT, across the two classes and across zones --- */
$a = new DateTime('@0');
$b = new DateTimeImmutable('@0');
dtcmp_show('DateTime == DateTimeImmutable', fn() => $a == $b);
dtcmp_show('DateTime <=> DateTimeImmutable', fn() => $a <=> $b);
dtcmp_show('DateTime === DateTimeImmutable', fn() => $a === $b);
dtcmp_show('later <=> earlier', fn() => (new DateTime('@100')) <=> $a);
dtcmp_show('earlier <=> later', fn() => $a <=> (new DateTime('@100')));
dtcmp_show('same instant, two zones', fn() =>
    (new DateTime('2020-01-01 00:00:00', new DateTimeZone('UTC')))
    == (new DateTime('2020-01-01 01:00:00', new DateTimeZone('+01:00'))));
dtcmp_show('microseconds break the tie', fn() =>
    (new DateTime('2020-01-01 00:00:00.000001')) <=> (new DateTime('2020-01-01 00:00:00.000002')));

/* the handler is inherited, and it never looks at a property */
class DtCmpKid extends DateTime { public $extra = 1; }
$k1 = new DtCmpKid('@0');
$k2 = new DtCmpKid('@0');
$k2->extra = 9;
dtcmp_show('subclass == subclass (extra differs)', fn() => $k1 == $k2);
dtcmp_show('subclass == DateTime', fn() => $k1 == $a);

/* a date against something that is not one is php's uncomparable: 1 BOTH ways,
 * which leaves every relational spelling false */
class DtCmpPlain { public $x = 1; }
$plain = new DtCmpPlain;
dtcmp_show('DateTime <=> plain object', fn() => $a <=> $plain);
dtcmp_show('plain object <=> DateTime', fn() => $plain <=> $a);
dtcmp_show('DateTime < plain object', fn() => $a < $plain);
dtcmp_show('DateTime > plain object', fn() => $a > $plain);

/* --- DateInterval: never comparable --- */
set_error_handler(function ($no, $msg) { echo "  WARN: $msg\n"; return true; });
$i1 = new DateInterval('P1D');
$i2 = new DateInterval('P1D');
dtcmp_show('P1D == P1D', fn() => $i1 == $i2);
dtcmp_show('P1D != P1D', fn() => $i1 != $i2);
dtcmp_show('P1D <=> P2D', fn() => $i1 <=> (new DateInterval('P2D')));
dtcmp_show('P1D < P2D', fn() => $i1 < (new DateInterval('P2D')));
dtcmp_show('P1D > P2D', fn() => $i1 > (new DateInterval('P2D')));
dtcmp_show('$i == $i (identity, no handler)', fn() => $i1 == $i1);
dtcmp_show('$i == clone $i', fn() => $i1 == clone $i1);
dtcmp_show('interval <=> plain object', fn() => $i1 <=> $plain);
dtcmp_show('in_array(interval)', fn() => in_array($i1, [$i2]));
restore_error_handler();

/* --- DateTimeZone: same kind or a refusal, and never an ordering --- */
$utc = new DateTimeZone('UTC');
$o1 = new DateTimeZone('+01:00');
$o2 = new DateTimeZone('+02:00');
dtcmp_show('UTC == UTC', fn() => $utc == new DateTimeZone('UTC'));
dtcmp_show('UTC === UTC', fn() => $utc === new DateTimeZone('UTC'));
dtcmp_show('+01:00 <=> +02:00', fn() => $o1 <=> $o2);
dtcmp_show('+01:00 < +02:00', fn() => $o1 < $o2);
dtcmp_show('+02:00 < +01:00', fn() => $o2 < $o1);
dtcmp_show('+01:00 == +0100', fn() => $o1 == new DateTimeZone('+0100'));
dtcmp_show('+00:00 == -00:00', fn() => (new DateTimeZone('+00:00')) == new DateTimeZone('-00:00'));
dtcmp_show('GMT <=> Z (both abbreviations)', fn() => (new DateTimeZone('GMT')) <=> new DateTimeZone('Z'));
dtcmp_show('GMT == gmt', fn() => (new DateTimeZone('GMT')) == new DateTimeZone('gmt'));
dtcmp_show('zone <=> plain object', fn() => $utc <=> $plain);

/* a refusal is raised from the comparison itself, wherever the comparison is */
dtcmp_show('UTC <=> +01:00 (kinds differ)', fn() => $utc <=> $o1);
dtcmp_show('UTC == GMT', fn() => $utc == new DateTimeZone('GMT'));
dtcmp_show('UTC < GMT', fn() => $utc < new DateTimeZone('GMT'));
dtcmp_show('in_array(zone, [other kind])', fn() => in_array($utc, [$o1]));
dtcmp_show('array_search(zone)', fn() => array_search($utc, [$o1]));
dtcmp_show('max(zone, other kind)', fn() => max($utc, $o1));
dtcmp_show('sort([zones of two kinds])', function () use ($utc, $o1) {
    $z = [$utc, $o1];
    sort($z);
    return count($z);
});
dtcmp_show('switch (zone) case other kind', function () use ($utc, $o1) {
    switch ($utc) {
        case $o1: return 'matched';
        default: return 'default';
    }
});
dtcmp_show('array_keys(zone search)', fn() => array_keys([$o1], $utc));
dtcmp_show('array_unique(zones)', fn() => count(array_unique([$utc, $o1], SORT_REGULAR)));
dtcmp_show('usort comparator', function () use ($utc, $o1) {
    $z = [$utc, $o1];
    usort($z, fn($x, $y) => $x <=> $y);
    return count($z);
});
dtcmp_show('a finally still runs', function () use ($utc, $o1) {
    try {
        return $utc == $o1;
    } finally {
        echo "  finally\n";
    }
});
dtcmp_show('caught by the INNER try', function () use ($utc, $o1) {
    try {
        try {
            return $utc == $o1;
        } catch (DateException $e) {
            return 'inner';
        }
    } catch (Throwable $e) {
        return 'outer';
    }
});
function dtcmp_gen($utc, $o1) {
    try {
        yield $utc == $o1;
    } catch (DateException $e) {
        yield 'caught in the generator';
    }
}
dtcmp_show('inside a generator body', fn() => dtcmp_gen($utc, $o1)->current());
/* the refusal does not outlive the comparison that made it */
dtcmp_show('a plain comparison after a refusal', fn() => 1 <=> 2);

/* --- DatePeriod: every one of them is equal to every other --- */
$p1 = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 3);
$p2 = new DatePeriod(new DateTime('@1000000'), new DateInterval('P2M'), 7);
dtcmp_show('period == other period', fn() => $p1 == $p2);
dtcmp_show('period <=> other period', fn() => $p1 <=> $p2);
dtcmp_show('period === other period', fn() => $p1 === $p2);
dtcmp_show('period < other period', fn() => $p1 < $p2);
dtcmp_show('period <=> plain object', fn() => $p1 <=> $plain);
/* ...but a SUBCLASS's own property is a real one, and still decides */
class DtCmpPeriodKid extends DatePeriod { public $extra = 1; }
$q1 = new DtCmpPeriodKid(new DateTime('@0'), new DateInterval('P1D'), 3);
$q2 = new DtCmpPeriodKid(new DateTime('@0'), new DateInterval('P1D'), 3);
dtcmp_show('period subclass == same', fn() => $q1 == $q2);
$q2->extra = 9;
dtcmp_show('period subclass == (extra differs)', fn() => $q1 == $q2);
dtcmp_show('period subclass <=> (extra differs)', fn() => $q1 <=> $q2);
?>
--EXPECT--
DateTime == DateTimeImmutable          true
DateTime <=> DateTimeImmutable         0
DateTime === DateTimeImmutable         false
later <=> earlier                      1
earlier <=> later                      -1
same instant, two zones                true
microseconds break the tie             -1
subclass == subclass (extra differs)   true
subclass == DateTime                   true
DateTime <=> plain object              1
plain object <=> DateTime              1
DateTime < plain object                false
DateTime > plain object                false
  WARN: Cannot compare DateInterval objects
P1D == P1D                             false
  WARN: Cannot compare DateInterval objects
P1D != P1D                             true
  WARN: Cannot compare DateInterval objects
P1D <=> P2D                            1
  WARN: Cannot compare DateInterval objects
P1D < P2D                              false
  WARN: Cannot compare DateInterval objects
P1D > P2D                              false
$i == $i (identity, no handler)        true
  WARN: Cannot compare DateInterval objects
$i == clone $i                         false
interval <=> plain object              1
  WARN: Cannot compare DateInterval objects
in_array(interval)                     false
UTC == UTC                             true
UTC === UTC                            false
+01:00 <=> +02:00                      1
+01:00 < +02:00                        false
+02:00 < +01:00                        false
+01:00 == +0100                        true
+00:00 == -00:00                       true
GMT <=> Z (both abbreviations)         1
GMT == gmt                             true
zone <=> plain object                  1
UTC <=> +01:00 (kinds differ)          DateException: Cannot compare two different kinds of DateTimeZone objects
UTC == GMT                             DateException: Cannot compare two different kinds of DateTimeZone objects
UTC < GMT                              DateException: Cannot compare two different kinds of DateTimeZone objects
in_array(zone, [other kind])           DateException: Cannot compare two different kinds of DateTimeZone objects
array_search(zone)                     DateException: Cannot compare two different kinds of DateTimeZone objects
max(zone, other kind)                  DateException: Cannot compare two different kinds of DateTimeZone objects
sort([zones of two kinds])             DateException: Cannot compare two different kinds of DateTimeZone objects
switch (zone) case other kind          DateException: Cannot compare two different kinds of DateTimeZone objects
array_keys(zone search)                DateException: Cannot compare two different kinds of DateTimeZone objects
array_unique(zones)                    DateException: Cannot compare two different kinds of DateTimeZone objects
usort comparator                       DateException: Cannot compare two different kinds of DateTimeZone objects
  finally
a finally still runs                   DateException: Cannot compare two different kinds of DateTimeZone objects
caught by the INNER try                'inner'
inside a generator body                'caught in the generator'
a plain comparison after a refusal     -1
period == other period                 true
period <=> other period                0
period === other period                false
period < other period                  false
period <=> plain object                1
period subclass == same                true
period subclass == (extra differs)     false
period subclass <=> (extra differs)    -1
