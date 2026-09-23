--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
serialize() writes a date object's PRESENTED shape, through __serialize/__unserialize
--DESCRIPTION--
php's DateTime/DateTimeImmutable/DateTimeZone keep their state in C, so serialize()
does not walk it: the classes declare an __serialize()/__unserialize() pair whose
payload is date/timezone_type/timezone, the same hash var_dump and var_export show.
PHL kept the state in HIDDEN slots and serialized THOSE, so a payload php wrote could
not be read back at all — the names it wanted were absent, the 1970 defaults survived,
and unserialize() answered a valid object holding the wrong instant instead of failing.
Both directions are the point of this test: PHL's own round trip, and a byte string php
produced. __wakeup() (the LEGACY payload, read out of the object's own properties) and
__set_state() (what var_export's text evaluates to) complete php's four, and all four
refuse ill-formed data with the same plain Error.
--FILE--
<?php
function dtSerShow($label, $fn) {
    try { $out = $fn(); if (!is_string($out)) { $out = var_export($out, true); } }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
$utc = new DateTimeZone('UTC');

echo "-- the payload is php's presented shape, not the engine slots\n";
dtSerShow('DateTime', fn() => serialize(new DateTime('2021-03-04 05:06:07.891234', $utc)));
dtSerShow('DateTimeImmutable',
    fn() => serialize(new DateTimeImmutable('2021-03-04 05:06:07', $utc)));
dtSerShow('DateTimeZone id', fn() => serialize(new DateTimeZone('UTC')));
dtSerShow('DateTimeZone offset', fn() => serialize(new DateTimeZone('+02:30')));
dtSerShow('__serialize()',
    fn() => json_encode((new DateTime('2021-03-04 05:06:07', $utc))->__serialize()));
dtSerShow('DateTimeZone::__serialize()',
    fn() => json_encode((new DateTimeZone('+02:30'))->__serialize()));

echo "-- and it round-trips, including a SUBCLASS's own properties\n";
class DtSerKid extends DateTime { public $extra = 'e'; }
dtSerShow('round trip',
    fn() => unserialize(serialize(new DateTime('2021-03-04 05:06:07', $utc)))->format('c'));
dtSerShow('subclass payload', fn() => serialize(new DtSerKid('2021-03-04 05:06:07', $utc)));
dtSerShow('subclass round trip', function () use ($utc) {
    $o = unserialize(serialize(new DtSerKid('2021-03-04 05:06:07', $utc)));
    return get_class($o) . '|' . $o->format('c') . '|' . $o->extra;
});
dtSerShow('zone round trip',
    fn() => unserialize(serialize(new DateTimeZone('+02:30')))->getName());

echo "-- a payload PHP wrote is readable, all three timezone_type tags\n";
dtSerShow('type 3 (identifier)', fn() => unserialize(
    'O:8:"DateTime":3:{s:4:"date";s:26:"2021-03-04 05:06:07.000000";'
    . 's:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";}')->format('c'));
dtSerShow('type 2 (abbreviation)', function () {
    $o = new DateTime;
    $o->__unserialize(['date' => '2021-03-04 05:06:07.000000',
        'timezone_type' => 2, 'timezone' => 'GMT']);
    return $o->format('c') . '|' . $o->getTimezone()->getName();
});
dtSerShow('type 1 (offset)', function () {
    $o = new DateTime;
    $o->__unserialize(['date' => '2021-03-04 05:06:07.000000',
        'timezone_type' => 1, 'timezone' => '+02:30']);
    return $o->format('c') . '|' . $o->getTimezone()->getName();
});
// The NAME decides, not the tag: php restores a UTC zone from a payload tagged 1.
dtSerShow('tag disagrees with name', function () {
    $o = new DateTime;
    $o->__unserialize(['date' => '2021-03-04 05:06:07.000000',
        'timezone_type' => 1, 'timezone' => 'UTC']);
    return $o->format('c') . '|' . $o->getTimezone()->getName();
});
dtSerShow('zone payload', fn() => unserialize(
    'O:12:"DateTimeZone":2:{s:13:"timezone_type";i:1;s:8:"timezone";s:6:"+02:30";}')
    ->getName());

echo "-- __set_state() is what var_export's text evaluates to\n";
dtSerShow('var_export', fn() => var_export(new DateTime('2021-03-04 05:06:07', $utc), true));
dtSerShow('__set_state', fn() => DateTime::__set_state(['date' => '2021-03-04 05:06:07.000000',
    'timezone_type' => 3, 'timezone' => 'UTC'])->format('c'));
dtSerShow('zone __set_state',
    fn() => DateTimeZone::__set_state(['timezone_type' => 1, 'timezone' => '+02:30'])->getName());
// php instantiates the DECLARING class there, never the called one.
dtSerShow('__set_state ignores the called class',
    fn() => get_class(DtSerKid::__set_state(['date' => '2021-03-04 05:06:07.000000',
        'timezone_type' => 3, 'timezone' => 'UTC'])));

echo "-- ill-formed data is one Error, whichever door it came through\n";
dtSerShow('missing keys', fn() => (new DateTime)->__unserialize(['x' => 1]));
dtSerShow('unknown zone', fn() => (new DateTime)->__unserialize(
    ['date' => '2021-03-04 05:06:07.000000', 'timezone_type' => 3, 'timezone' => 'No/Such']));
dtSerShow('tag out of range', fn() => (new DateTimeZone('UTC'))->__unserialize(
    ['timezone_type' => 4, 'timezone' => 'UTC']));
dtSerShow('tag is a string', fn() => (new DateTimeZone('UTC'))->__unserialize(
    ['timezone_type' => '3', 'timezone' => 'UTC']));
dtSerShow('__set_state refuses', fn() => DateTime::__set_state(['nope' => 1]));
// __wakeup reads the object's OWN properties, so a plain `new` is the failure case.
dtSerShow('__wakeup with nothing to read', fn() => (new DateTime)->__wakeup());
dtSerShow('not an array', fn() => (new DateTime)->__unserialize(1));
--EXPECT--
-- the payload is php's presented shape, not the engine slots
DateTime => O:8:"DateTime":3:{s:4:"date";s:26:"2021-03-04 05:06:07.891234";s:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";}
DateTimeImmutable => O:17:"DateTimeImmutable":3:{s:4:"date";s:26:"2021-03-04 05:06:07.000000";s:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";}
DateTimeZone id => O:12:"DateTimeZone":2:{s:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";}
DateTimeZone offset => O:12:"DateTimeZone":2:{s:13:"timezone_type";i:1;s:8:"timezone";s:6:"+02:30";}
__serialize() => {"date":"2021-03-04 05:06:07.000000","timezone_type":3,"timezone":"UTC"}
DateTimeZone::__serialize() => {"timezone_type":1,"timezone":"+02:30"}
-- and it round-trips, including a SUBCLASS's own properties
round trip => 2021-03-04T05:06:07+00:00
subclass payload => O:8:"DtSerKid":4:{s:4:"date";s:26:"2021-03-04 05:06:07.000000";s:13:"timezone_type";i:3;s:8:"timezone";s:3:"UTC";s:5:"extra";s:1:"e";}
subclass round trip => DtSerKid|2021-03-04T05:06:07+00:00|e
zone round trip => +02:30
-- a payload PHP wrote is readable, all three timezone_type tags
type 3 (identifier) => 2021-03-04T05:06:07+00:00
type 2 (abbreviation) => 2021-03-04T05:06:07+00:00|GMT
type 1 (offset) => 2021-03-04T05:06:07+02:30|+02:30
tag disagrees with name => 2021-03-04T05:06:07+00:00|UTC
zone payload => +02:30
-- __set_state() is what var_export's text evaluates to
var_export => \DateTime::__set_state(array(   'date' => '2021-03-04 05:06:07.000000',   'timezone_type' => 3,   'timezone' => 'UTC',))
__set_state => 2021-03-04T05:06:07+00:00
zone __set_state => +02:30
__set_state ignores the called class => DateTime
-- ill-formed data is one Error, whichever door it came through
missing keys => Error: Invalid serialization data for DateTime object
unknown zone => Error: Invalid serialization data for DateTime object
tag out of range => Error: Invalid serialization data for DateTimeZone object
tag is a string => Error: Invalid serialization data for DateTimeZone object
__set_state refuses => Error: Invalid serialization data for DateTime object
__wakeup with nothing to read => Error: Invalid serialization data for DateTime object
not an array => TypeError: DateTime::__unserialize(): Argument #1 ($data) must be of type array, int given
