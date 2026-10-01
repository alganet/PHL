--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every door of a date object that was never constructed raises DateObjectError
--FILE--
<?php
/* php keeps a date's state in a C struct the CONSTRUCTOR allocates, so an object
 * that never ran one -- what newInstanceWithoutConstructor() answers, and what a
 * subclass whose own constructor forgets parent::__construct() IS -- has no state
 * at all and every door raises DateObjectError. PHL's state lives in slots that
 * hold their defaults from instantiation, so such an object silently WAS
 * 1970-01-01 UTC: every getter, every mutator, every procedural alias and every
 * factory answered a date nothing in the program had asked for.
 *
 * The doors php lets through are here too: the constructors (which is what
 * re-initializing an object means), __unserialize/__wakeup, DatePeriod's two
 * nullable getters, and setTimezone(), whose fallback for a zone with no struct
 * is a fixed +00:00. */
date_default_timezone_set('UTC');

class DtUninitSub extends DateTime { public function __construct() {} }
class DtUninitSubSub extends DtUninitSub { public function __construct() {} }
class DtUninitZoneSub extends DateTimeZone { public function __construct() {} }
class DtUninitIvSub extends DateInterval { public function __construct() {} }

function dtuninit_new($class) {
    return (new ReflectionClass($class))->newInstanceWithoutConstructor();
}
function dtuninit_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-40s %s\n", $label, is_object($r) ? get_class($r) : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-40s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}

/* --- every METHOD of an unconstructed DateTime --- */
$d = dtuninit_new('DateTime');
dtuninit_show('format', fn() => $d->format('Y-m-d'));
dtuninit_show('getTimestamp', fn() => $d->getTimestamp());
dtuninit_show('getOffset', fn() => $d->getOffset());
dtuninit_show('getMicrosecond', fn() => $d->getMicrosecond());
dtuninit_show('getTimezone', fn() => $d->getTimezone());
dtuninit_show('modify', fn() => $d->modify('+1 day'));
dtuninit_show('setTimestamp', fn() => $d->setTimestamp(0));
dtuninit_show('setMicrosecond', fn() => $d->setMicrosecond(5));
dtuninit_show('setDate', fn() => $d->setDate(2020, 1, 1));
dtuninit_show('setTime', fn() => $d->setTime(1, 1));
dtuninit_show('setISODate', fn() => $d->setISODate(2020, 1));
dtuninit_show('setTimezone', fn() => $d->setTimezone(new DateTimeZone('UTC')));
dtuninit_show('add', fn() => $d->add(new DateInterval('P1D')));
dtuninit_show('sub', fn() => $d->sub(new DateInterval('P1D')));
dtuninit_show('diff', fn() => $d->diff(new DateTime('@0')));
dtuninit_show('__serialize', fn() => $d->__serialize());

/* the same object as an ARGUMENT: php screens what it is HANDED too */
dtuninit_show('fresh->diff(uninitialized)', fn() => (new DateTime('@0'))->diff($d));
dtuninit_show('createFromInterface', fn() => DateTime::createFromInterface($d));
dtuninit_show('createFromMutable', fn() => DateTimeImmutable::createFromMutable($d));
dtuninit_show('DateTimeZone::getOffset', fn() => (new DateTimeZone('UTC'))->getOffset($d));

/* the whole procedural surface reaches the same implementation */
dtuninit_show('date_format', fn() => date_format($d, 'Y'));
dtuninit_show('date_modify', fn() => date_modify($d, '+1 day'));
dtuninit_show('date_timestamp_get', fn() => date_timestamp_get($d));
dtuninit_show('date_timestamp_set', fn() => date_timestamp_set($d, 0));
dtuninit_show('date_timezone_get', fn() => date_timezone_get($d));
dtuninit_show('date_offset_get', fn() => date_offset_get($d));
dtuninit_show('date_date_set', fn() => date_date_set($d, 2020, 1, 1));
dtuninit_show('date_time_set', fn() => date_time_set($d, 1, 1));
dtuninit_show('date_isodate_set', fn() => date_isodate_set($d, 2020, 1));
dtuninit_show('date_diff', fn() => date_diff(new DateTime('@0'), $d));
dtuninit_show('timezone_offset_get', fn() => timezone_offset_get(new DateTimeZone('UTC'), $d));

/* a SUBCLASS names the class it inherits, at any depth of the chain */
dtuninit_show('subclass', fn() => (new DtUninitSub)->format('Y'));
dtuninit_show('subclass of a subclass', fn() => (new DtUninitSubSub)->format('Y'));
dtuninit_show('DateTimeImmutable', fn() => dtuninit_new('DateTimeImmutable')->modify('+1 day'));

/* a clone of one is another; a constructor CALL is what fixes both */
dtuninit_show('clone', fn() => (clone $d)->format('Y'));
dtuninit_show('__construct() then format', function () use ($d) {
    $d->__construct('@5');
    return $d->format('Y-m-d H:i:s');
});
dtuninit_show('a failed __construct leaves it', function () {
    $u = dtuninit_new('DateTime');
    try { $u->__construct('not a time'); } catch (Throwable $e) {}
    return $u->format('Y');
});

/* --- DateTimeZone --- */
$z = dtuninit_new('DateTimeZone');
dtuninit_show('zone getName', fn() => $z->getName());
dtuninit_show('zone getOffset', fn() => $z->getOffset(new DateTime('@0')));
dtuninit_show('zone __serialize', fn() => $z->__serialize());
dtuninit_show('timezone_name_get', fn() => timezone_name_get($z));
dtuninit_show('timezone_offset_get(zone)', fn() => timezone_offset_get($z, new DateTime('@0')));
dtuninit_show('zone subclass', fn() => (new DtUninitZoneSub)->getName());
/* the one door that does NOT raise: php's fallback zone is a fixed +00:00 */
dtuninit_show('setTimezone(uninitialized)', fn() => (new DateTime('@0'))->setTimezone($z)->format('P e'));
dtuninit_show('date_timezone_set(uninitialized)', fn() => date_timezone_set(new DateTime('@0'), $z)->format('P e'));
/* while BUILDING a date from one is a plain Error with its own sentence */
dtuninit_show('new DateTime(now, uninitialized)', fn() => (new DateTime('now', $z))->format('P'));
dtuninit_show('new DateTimeImmutable(uninit zone)', fn() => (new DateTimeImmutable('now', $z))->format('P'));
dtuninit_show('date_create(now, uninitialized)', fn() => date_create('now', $z)->format('P'));
dtuninit_show('createFromFormat(uninit zone)', fn() => DateTime::createFromFormat('Y', '2020', $z)->format('P'));

/* --- DateInterval --- */
$i = dtuninit_new('DateInterval');
dtuninit_show('interval format', fn() => $i->format('%d'));
dtuninit_show('interval subclass format', fn() => (new DtUninitIvSub)->format('%d'));
dtuninit_show('date_interval_format', fn() => date_interval_format($i, '%d'));
dtuninit_show('fresh->add(uninitialized)', fn() => (new DateTime('@0'))->add($i));
dtuninit_show('fresh->sub(uninitialized)', fn() => (new DateTime('@0'))->sub($i));
dtuninit_show('date_add(uninitialized)', fn() => date_add(new DateTime('@0'), $i));

/* --- DatePeriod --- */
$p = dtuninit_new('DatePeriod');
dtuninit_show('period getStartDate', fn() => $p->getStartDate());
dtuninit_show('period getDateInterval', fn() => $p->getDateInterval());
dtuninit_show('period foreach', function () use ($p) {
    foreach ($p as $x) {}
    return 'walked';
});
/* php's two NULLABLE getters read the struct without screening it */
dtuninit_show('period getEndDate', fn() => $p->getEndDate());
dtuninit_show('period getRecurrences', fn() => $p->getRecurrences());
/* and an unconstructed date on either end of a real one is refused by the name
 * of the parameter's DECLARED type */
dtuninit_show('new DatePeriod(uninit start)',
    fn() => new DatePeriod(dtuninit_new('DateTime'), new DateInterval('P1D'), 1));
dtuninit_show('new DatePeriod(uninit end)',
    fn() => new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), dtuninit_new('DateTime')));
/* (php SEGFAULTS on an unconstructed INTERVAL in the same constructor -- it
 * screens the two dates and not the interval -- so that case is deliberately not
 * spelled here: PHL refuses it the way every other door does, the scope policy.) */

/* --- what a constructor is not the only way to build --- */
dtuninit_show('date_create is initialized', fn() => date_create('@5')->format('U'));
dtuninit_show('createFromTimestamp', fn() => DateTime::createFromTimestamp(5)->format('U'));
dtuninit_show('createFromFormat', fn() => DateTime::createFromFormat('U', '5')->format('U'));
dtuninit_show('getTimezone answers a usable zone',
    fn() => (new DateTime('@0'))->getTimezone()->getName());
dtuninit_show('diff answers a usable interval',
    fn() => (new DateTime('@0'))->diff(new DateTime('@86400'))->format('%d'));
dtuninit_show('createFromDateString', fn() => DateInterval::createFromDateString('2 days')->format('%d'));
dtuninit_show('timezone_open', fn() => timezone_open('+01:00')->getName());
dtuninit_show('__set_state', fn() => DateTime::__set_state([
    'date' => '1970-01-01 00:00:05.000000', 'timezone_type' => 1, 'timezone' => '+00:00',
])->format('U'));
dtuninit_show('period ISO factory',
    fn() => DatePeriod::createFromISO8601String('R2/1970-01-01T00:00:00Z/P1D')->getStartDate()->format('U'));

/* unserialize() re-initializes through the magic php declares for it */
dtuninit_show('unserialize(serialize(date))', fn() => unserialize(serialize(new DateTime('@5')))->format('U'));
dtuninit_show('unserialize(serialize(zone))', fn() => unserialize(serialize(new DateTimeZone('+01:00')))->getName());
dtuninit_show('unserialize(serialize(interval))',
    fn() => unserialize(serialize(new DateInterval('P3D')))->format('%d'));
dtuninit_show('unserialize(serialize(period))',
    fn() => count(iterator_to_array(unserialize(serialize(
        new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2))))));
dtuninit_show('__unserialize on a bare object', function () {
    $u = dtuninit_new('DateTime');
    $u->__unserialize(['date' => '1970-01-01 00:00:07.000000', 'timezone_type' => 1, 'timezone' => '+00:00']);
    return $u->format('U');
});
dtuninit_show('__wakeup with no payload', fn() => @dtuninit_new('DateTime')->__wakeup());
--EXPECT--
format                                   DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
getTimestamp                             DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
getOffset                                DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
getMicrosecond                           DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
getTimezone                              DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
modify                                   DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
setTimestamp                             DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
setMicrosecond                           DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
setDate                                  DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
setTime                                  DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
setISODate                               DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
setTimezone                              DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
add                                      DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
sub                                      DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
diff                                     DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
__serialize                              DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
fresh->diff(uninitialized)               DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
createFromInterface                      DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
createFromMutable                        DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
DateTimeZone::getOffset                  DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_format                              DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_modify                              DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_timestamp_get                       DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_timestamp_set                       DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_timezone_get                        DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_offset_get                          DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_date_set                            DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_time_set                            DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_isodate_set                         DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
date_diff                                DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
timezone_offset_get                      DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
subclass                                 DateObjectError: Object of type DtUninitSub (inheriting DateTime) has not been correctly initialized by calling parent::__construct() in its constructor
subclass of a subclass                   DateObjectError: Object of type DtUninitSubSub (inheriting DateTime) has not been correctly initialized by calling parent::__construct() in its constructor
DateTimeImmutable                        DateObjectError: Object of type DateTimeImmutable has not been correctly initialized by calling parent::__construct() in its constructor
clone                                    DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
__construct() then format                '1970-01-01 00:00:05'
a failed __construct leaves it           DateObjectError: Object of type DateTime has not been correctly initialized by calling parent::__construct() in its constructor
zone getName                             DateObjectError: Object of type DateTimeZone has not been correctly initialized by calling parent::__construct() in its constructor
zone getOffset                           DateObjectError: Object of type DateTimeZone has not been correctly initialized by calling parent::__construct() in its constructor
zone __serialize                         DateObjectError: Object of type DateTimeZone has not been correctly initialized by calling parent::__construct() in its constructor
timezone_name_get                        DateObjectError: Object of type DateTimeZone has not been correctly initialized by calling parent::__construct() in its constructor
timezone_offset_get(zone)                DateObjectError: Object of type DateTimeZone has not been correctly initialized by calling parent::__construct() in its constructor
zone subclass                            DateObjectError: Object of type DtUninitZoneSub (inheriting DateTimeZone) has not been correctly initialized by calling parent::__construct() in its constructor
setTimezone(uninitialized)               '+00:00 +00:00'
date_timezone_set(uninitialized)         '+00:00 +00:00'
new DateTime(now, uninitialized)         Error: The DateTimeZone object has not been correctly initialized by its constructor
new DateTimeImmutable(uninit zone)       Error: The DateTimeZone object has not been correctly initialized by its constructor
date_create(now, uninitialized)          Error: The DateTimeZone object has not been correctly initialized by its constructor
createFromFormat(uninit zone)            Error: The DateTimeZone object has not been correctly initialized by its constructor
interval format                          DateObjectError: Object of type DateInterval has not been correctly initialized by calling parent::__construct() in its constructor
interval subclass format                 DateObjectError: Object of type DtUninitIvSub (inheriting DateInterval) has not been correctly initialized by calling parent::__construct() in its constructor
date_interval_format                     DateObjectError: Object of type DateInterval has not been correctly initialized by calling parent::__construct() in its constructor
fresh->add(uninitialized)                DateObjectError: Object of type DateInterval has not been correctly initialized by calling parent::__construct() in its constructor
fresh->sub(uninitialized)                DateObjectError: Object of type DateInterval has not been correctly initialized by calling parent::__construct() in its constructor
date_add(uninitialized)                  DateObjectError: Object of type DateInterval has not been correctly initialized by calling parent::__construct() in its constructor
period getStartDate                      DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
period getDateInterval                   DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
period foreach                           DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
period getEndDate                        NULL
period getRecurrences                    NULL
new DatePeriod(uninit start)             DateObjectError: Object of type DateTimeInterface has not been correctly initialized by calling parent::__construct() in its constructor
new DatePeriod(uninit end)               DateObjectError: Object of type DateTimeInterface has not been correctly initialized by calling parent::__construct() in its constructor
date_create is initialized               '5'
createFromTimestamp                      '5'
createFromFormat                         '5'
getTimezone answers a usable zone        '+00:00'
diff answers a usable interval           '1'
createFromDateString                     '2'
timezone_open                            '+01:00'
__set_state                              '5'
period ISO factory                       '0'
unserialize(serialize(date))             '5'
unserialize(serialize(zone))             '+01:00'
unserialize(serialize(interval))         '3'
unserialize(serialize(period))           3
__unserialize on a bare object           '7'
__wakeup with no payload                 Error: Invalid serialization data for DateTime object
