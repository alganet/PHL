--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unconstructed date SHOWS nothing and refuses to be compared
--FILE--
<?php
/* The two surfaces that read a date's state without going through a method.
 *
 * php builds the shape it SHOWS from the struct its constructor allocates, so an
 * object that never ran one shows nothing at all: var_dump, print_r, var_export,
 * the (array) cast, get_object_vars() and json_encode() are all empty. PHL built
 * that shape out of slots holding their defaults, so every one of them published
 * a 1970-01-01 UTC date the program had never asked for.
 *
 * And php's compare handlers refuse an unconstructed operand outright, from
 * EITHER side and with a different sentence per class — where PHL compared the
 * defaults and answered that two unconstructed dates were equal, and that one of
 * them equalled the epoch. That refusal reaches every comparison DRIVER too:
 * sort(), in_array() and switch all go through the same comparator. */
date_default_timezone_set('UTC');

function dtsurf_new($class) {
    return (new ReflectionClass($class))->newInstanceWithoutConstructor();
}
function dtsurf_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-34s %s\n", $label, is_string($r) ? $r : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-34s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}
function dtsurf_dump($label, $v) {
    ob_start();
    var_dump($v);
    /* the object HANDLE never matches between two engines: what matters here is
     * the entry COUNT, which is zero */
    dtsurf_show($label, fn() => preg_replace('/#\d+ /', '#N ', trim(ob_get_clean())));
}

/* --- what it SHOWS --- */
$d = dtsurf_new('DateTime');
$i = dtsurf_new('DateTimeImmutable');
$z = dtsurf_new('DateTimeZone');
dtsurf_dump('var_dump', $d);
dtsurf_show('print_r', fn() => trim(print_r($d, true)));
dtsurf_show('var_export', fn() => var_export($d, true));
dtsurf_show('(array)', fn() => (array)$d);
dtsurf_show('get_object_vars', fn() => get_object_vars($d));
dtsurf_show('json_encode', fn() => json_encode($d));
dtsurf_show('count of (array)', fn() => count((array)$d));
dtsurf_dump('immutable var_dump', $i);
dtsurf_show('immutable json', fn() => json_encode($i));
dtsurf_dump('zone var_dump', $z);
dtsurf_show('zone (array)', fn() => (array)$z);
dtsurf_show('zone json', fn() => json_encode($z));
dtsurf_show('zone var_export', fn() => var_export($z, true));

/* a CONSTRUCTED one still shows php's three keys, and the (array) cast of a
 * subclass still answers them */
dtsurf_show('constructed (array)', fn() => (array)new DateTime('@0'));
dtsurf_show('constructed json', fn() => json_encode(new DateTime('@0')));
dtsurf_show('zone constructed (array)', fn() => (array)new DateTimeZone('+01:00'));
dtsurf_show('re-constructed shows again', function () {
    $u = dtsurf_new('DateTime');
    $u->__construct('@0');
    return json_encode($u);
});

/* --- how it COMPARES --- */
$a = dtsurf_new('DateTime');
$b = dtsurf_new('DateTime');
dtsurf_show('uninitialized == itself', fn() => $a == $a);           /* php's identity shortcut */
dtsurf_show('uninitialized === itself', fn() => $a === $a);
dtsurf_show('uninitialized == another', fn() => $a == $b);
dtsurf_show('uninitialized == epoch', fn() => $a == new DateTime('@0'));
dtsurf_show('epoch == uninitialized', fn() => new DateTime('@0') == $a);
dtsurf_show('uninitialized < epoch', fn() => $a < new DateTime('@0'));
dtsurf_show('uninitialized <=> immutable', fn() => $a <=> new DateTimeImmutable('@0'));
/* a date against something that is not one stays php's silent uncomparable */
dtsurf_show('uninitialized <=> plain object', fn() => $a <=> new stdClass);
dtsurf_show('uninitialized == a zone', fn() => $a == new DateTimeZone('UTC'));
/* every driver of the comparator, not only the operators */
dtsurf_show('sort()', function () use ($a) {
    $arr = [$a, new DateTime('@0')];
    sort($arr);
    return count($arr);
});
dtsurf_show('in_array()', fn() => in_array($a, [new DateTime('@0')]));
dtsurf_show('switch', function () use ($a) {
    switch ($a) {
        case new DateTime('@0'): return 'matched';
        default: return 'fell through';
    }
});
/* the zone half has its own sentence, and the interval half is the ordinary
 * "cannot compare" warning whether or not either side was constructed */
$uz = dtsurf_new('DateTimeZone');
dtsurf_show('zone == zone', fn() => $uz == dtsurf_new('DateTimeZone'));
dtsurf_show('zone == UTC', fn() => $uz == new DateTimeZone('UTC'));
dtsurf_show('UTC == zone', fn() => new DateTimeZone('UTC') == $uz);
dtsurf_show('zone <=> plain object', fn() => $uz <=> new stdClass);
set_error_handler(function ($no, $msg) { echo "  WARN: $msg\n"; return true; });
dtsurf_show('interval == interval', fn() => dtsurf_new('DateInterval') == dtsurf_new('DateInterval'));
restore_error_handler();
/* two DatePeriods are equal whatever they hold — php's real property table for
 * one is empty, constructed or not */
dtsurf_show('period == period', fn() => dtsurf_new('DatePeriod') == dtsurf_new('DatePeriod'));
--EXPECT--
var_dump                           object(DateTime)#N (0) {
}
print_r                            DateTime Object
(
)
var_export                         \DateTime::__set_state(array(
))
(array)                            array (
)
get_object_vars                    array (
)
json_encode                        {}
count of (array)                   0
immutable var_dump                 object(DateTimeImmutable)#N (0) {
}
immutable json                     {}
zone var_dump                      object(DateTimeZone)#N (0) {
}
zone (array)                       array (
)
zone json                          {}
zone var_export                    \DateTimeZone::__set_state(array(
))
constructed (array)                array (
  'date' => '1970-01-01 00:00:00.000000',
  'timezone_type' => 1,
  'timezone' => '+00:00',
)
constructed json                   {"date":"1970-01-01 00:00:00.000000","timezone_type":1,"timezone":"+00:00"}
zone constructed (array)           array (
  'timezone_type' => 1,
  'timezone' => '+01:00',
)
re-constructed shows again         {"date":"1970-01-01 00:00:00.000000","timezone_type":1,"timezone":"+00:00"}
uninitialized == itself            true
uninitialized === itself           true
uninitialized == another           DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
uninitialized == epoch             DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
epoch == uninitialized             DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
uninitialized < epoch              DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
uninitialized <=> immutable        DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
uninitialized <=> plain object     1
uninitialized == a zone            false
sort()                             DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
in_array()                         DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
switch                             DateObjectError: Trying to compare an incomplete DateTime or DateTimeImmutable object
zone == zone                       DateObjectError: Trying to compare uninitialized DateTimeZone objects
zone == UTC                        DateObjectError: Trying to compare uninitialized DateTimeZone objects
UTC == zone                        DateObjectError: Trying to compare uninitialized DateTimeZone objects
zone <=> plain object              1
  WARN: Cannot compare DateInterval objects
interval == interval               false
period == period                   true
