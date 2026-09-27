--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DateInterval and DatePeriod carry php's serialization quartet
--FILE--
<?php
/* php declares __serialize/__unserialize/__wakeup/__set_state on both classes and
 * PHL declared none of them: `serialize()` walked the slots by luck (for these two
 * the state IS the php-visible property table, so the bytes matched),
 * `var_export()`'s `\DateInterval::__set_state(array(...))` text evaluated to
 * "Call to undefined method", and serializing an object that was never
 * constructed answered a payload where php raises.
 *
 * The two RESTORE rules are different rules, and both are php's: an interval
 * fills a MISSING field with -1 — timelib's "unset" marker — and refuses nothing,
 * while a period refuses any payload that is not complete and well-typed. */
date_default_timezone_set('UTC');

function dtser_new($class) {
    return (new ReflectionClass($class))->newInstanceWithoutConstructor();
}
function dtser_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-36s %s\n", $label, is_string($r) ? $r : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-36s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}
function dtser_dump($label, $fn) {
    dtser_show($label, function () use ($fn) {
        ob_start();
        var_dump($fn());
        /* object HANDLES never match between two engines */
        return preg_replace('/#\d+ /', '#N ', trim(ob_get_clean()));
    });
}

/* --- the methods exist and answer php's payload --- */
foreach (['DateInterval', 'DatePeriod'] as $c) {
    foreach (['__serialize', '__unserialize', '__wakeup', '__set_state'] as $m) {
        dtser_show("$c::$m exists", fn() => method_exists($c, $m));
    }
}
dtser_show('interval __serialize', fn() => (new DateInterval('P1D'))->__serialize());
dtser_show('period __serialize keys',
    fn() => array_keys((new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2))->__serialize()));

/* --- the bytes, and the round trip --- */
dtser_show('serialize(interval)', fn() => serialize(new DateInterval('P1D')));
dtser_show('serialize(period)',
    fn() => serialize(new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2)));
dtser_dump('unserialize(serialize(interval))', fn() => unserialize(serialize(new DateInterval('P1D'))));
dtser_dump('unserialize(diff interval)',
    fn() => unserialize(serialize((new DateTime('@0'))->diff(new DateTime('@86400')))));
dtser_show('unserialize(period) walks',
    fn() => count(iterator_to_array(unserialize(serialize(
        new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2))))));

/* --- var_export's text really evaluates back --- */
dtser_show('var_export(interval)', fn() => var_export(new DateInterval('P2D'), true));
dtser_show('__set_state(interval)', function () {
    $o = eval('return ' . var_export(new DateInterval('P2D'), true) . ';');
    return $o->format('%d');
});
dtser_show('__set_state(period)', function () {
    $o = eval('return ' . var_export(new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2), true) . ';');
    return count(iterator_to_array($o));
});
dtser_show('__set_state(interval, partial)', fn() => DateInterval::__set_state(['d' => 4])->format('%d %y'));
dtser_show('__set_state(period, partial)', fn() => DatePeriod::__set_state(['recurrences' => 2]));
dtser_show('__set_state(interval, scalar)', fn() => DateInterval::__set_state(5));

/* --- an interval fills what is missing with -1, and refuses nothing --- */
dtser_dump('__unserialize([])', function () {
    $o = dtser_new('DateInterval');
    $o->__unserialize([]);
    return $o;
});
dtser_show('__unserialize partial', function () {
    $o = dtser_new('DateInterval');
    $o->__unserialize(['d' => 3]);
    return $o->format('%d %y %h');
});
dtser_show('__unserialize junk value', function () {
    $o = dtser_new('DateInterval');
    $o->__unserialize(['d' => 'x', 'y' => [1]]);
    return $o->format('%d %y');
});
dtser_show('__unserialize keeps days false', function () {
    $o = dtser_new('DateInterval');
    $o->__unserialize(['days' => false]);
    return var_export($o->days, true);
});
dtser_show('__unserialize days count', function () {
    $o = dtser_new('DateInterval');
    $o->__unserialize(['days' => 5]);
    return var_export($o->days, true);
});
dtser_show('__unserialize on a live one', function () {
    $o = new DateInterval('P9D');
    $o->__unserialize(['d' => 3]);
    return $o->format('%d');
});
dtser_show('__unserialize non-array', function () {
    $o = dtser_new('DateInterval');
    $o->__unserialize(5);
    return 'accepted';
});
dtser_show('legacy payload, no fields',
    fn() => unserialize('O:12:"DateInterval":0:{}')->format('%y %m %d'));

/* --- a period refuses an incomplete payload, whichever door it arrives at --- */
$good = (new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2))->__serialize();
foreach (['start', 'interval', 'recurrences', 'include_end_date'] as $k) {
    dtser_show("period without $k", function () use ($good, $k) {
        $d = $good;
        unset($d[$k]);
        $o = dtser_new('DatePeriod');
        $o->__unserialize($d);
        return 'accepted';
    });
    dtser_show("period with junk $k", function () use ($good, $k) {
        $d = $good;
        $d[$k] = 'junk';
        $o = dtser_new('DatePeriod');
        $o->__unserialize($d);
        return 'accepted';
    });
}
dtser_show('period with a null start', function () use ($good) {
    $d = $good;
    $d['start'] = null;
    $o = dtser_new('DatePeriod');
    $o->__unserialize($d);
    return 'accepted';
});
dtser_show('legacy period payload', fn() => unserialize('O:10:"DatePeriod":0:{}'));

/* --- and an object that was never constructed cannot be serialized at all --- */
dtser_show('serialize(uninitialized interval)', fn() => serialize(dtser_new('DateInterval')));
dtser_show('serialize(uninitialized period)', fn() => serialize(dtser_new('DatePeriod')));
dtser_show('uninitialized __serialize', fn() => dtser_new('DateInterval')->__serialize());
--EXPECT--
DateInterval::__serialize exists     true
DateInterval::__unserialize exists   true
DateInterval::__wakeup exists        true
DateInterval::__set_state exists     true
DatePeriod::__serialize exists       true
DatePeriod::__unserialize exists     true
DatePeriod::__wakeup exists          true
DatePeriod::__set_state exists       true
interval __serialize                 array (
  'y' => 0,
  'm' => 0,
  'd' => 1,
  'h' => 0,
  'i' => 0,
  's' => 0,
  'f' => 0.0,
  'invert' => 0,
  'days' => false,
  'from_string' => false,
)
period __serialize keys              array (
  0 => 'start',
  1 => 'current',
  2 => 'end',
  3 => 'interval',
  4 => 'recurrences',
  5 => 'include_start_date',
  6 => 'include_end_date',
)
serialize(interval)                  O:12:"DateInterval":10:{s:1:"y";i:0;s:1:"m";i:0;s:1:"d";i:1;s:1:"h";i:0;s:1:"i";i:0;s:1:"s";i:0;s:1:"f";d:0;s:6:"invert";i:0;s:4:"days";b:0;s:11:"from_string";b:0;}
serialize(period)                    O:10:"DatePeriod":7:{s:5:"start";O:8:"DateTime":3:{s:4:"date";s:26:"1970-01-01 00:00:00.000000";s:13:"timezone_type";i:1;s:8:"timezone";s:6:"+00:00";}s:7:"current";N;s:3:"end";N;s:8:"interval";O:12:"DateInterval":10:{s:1:"y";i:0;s:1:"m";i:0;s:1:"d";i:1;s:1:"h";i:0;s:1:"i";i:0;s:1:"s";i:0;s:1:"f";d:0;s:6:"invert";i:0;s:4:"days";b:0;s:11:"from_string";b:0;}s:11:"recurrences";i:3;s:18:"include_start_date";b:1;s:16:"include_end_date";b:0;}
unserialize(serialize(interval))     object(DateInterval)#N (10) {
  ["y"]=>
  int(0)
  ["m"]=>
  int(0)
  ["d"]=>
  int(1)
  ["h"]=>
  int(0)
  ["i"]=>
  int(0)
  ["s"]=>
  int(0)
  ["f"]=>
  float(0)
  ["invert"]=>
  int(0)
  ["days"]=>
  bool(false)
  ["from_string"]=>
  bool(false)
}
unserialize(diff interval)           object(DateInterval)#N (10) {
  ["y"]=>
  int(0)
  ["m"]=>
  int(0)
  ["d"]=>
  int(1)
  ["h"]=>
  int(0)
  ["i"]=>
  int(0)
  ["s"]=>
  int(0)
  ["f"]=>
  float(0)
  ["invert"]=>
  int(0)
  ["days"]=>
  int(1)
  ["from_string"]=>
  bool(false)
}
unserialize(period) walks            3
var_export(interval)                 \DateInterval::__set_state(array(
   'y' => 0,
   'm' => 0,
   'd' => 2,
   'h' => 0,
   'i' => 0,
   's' => 0,
   'f' => 0.0,
   'invert' => 0,
   'days' => false,
   'from_string' => false,
))
__set_state(interval)                2
__set_state(period)                  3
__set_state(interval, partial)       4 -1
__set_state(period, partial)         Error: Invalid serialization data for DatePeriod object
__set_state(interval, scalar)        TypeError: DateInterval::__set_state(): Argument #1 ($array) must be of type array, int given
__unserialize([])                    object(DateInterval)#N (10) {
  ["y"]=>
  int(-1)
  ["m"]=>
  int(-1)
  ["d"]=>
  int(-1)
  ["h"]=>
  int(-1)
  ["i"]=>
  int(-1)
  ["s"]=>
  int(-1)
  ["f"]=>
  float(0)
  ["invert"]=>
  int(0)
  ["days"]=>
  int(-1)
  ["from_string"]=>
  bool(false)
}
__unserialize partial                3 -1 -1
__unserialize junk value             0 -1
__unserialize keeps days false       false
__unserialize days count             5
__unserialize on a live one          3
__unserialize non-array              TypeError: DateInterval::__unserialize(): Argument #1 ($data) must be of type array, int given
legacy payload, no fields            -1 -1 -1
period without start                 Error: Invalid serialization data for DatePeriod object
period with junk start               Error: Invalid serialization data for DatePeriod object
period without interval              Error: Invalid serialization data for DatePeriod object
period with junk interval            Error: Invalid serialization data for DatePeriod object
period without recurrences           Error: Invalid serialization data for DatePeriod object
period with junk recurrences         Error: Invalid serialization data for DatePeriod object
period without include_end_date      Error: Invalid serialization data for DatePeriod object
period with junk include_end_date    Error: Invalid serialization data for DatePeriod object
period with a null start             accepted
legacy period payload                Error: Invalid serialization data for DatePeriod object
serialize(uninitialized interval)    DateObjectError: Object of type DateInterval has not been correctly initialized by calling parent::__construct() in its constructor
serialize(uninitialized period)      DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
uninitialized __serialize            DateObjectError: Object of type DateInterval has not been correctly initialized by calling parent::__construct() in its constructor
