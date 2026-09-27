--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unconstructed interval or period shows nothing, and reads php's zeroed struct
--FILE--
<?php
/* DateInterval and DatePeriod keep their state where php keeps it too — in the
 * properties themselves — so the presentation hook exists for one reason: php
 * has NO property table there until the object is constructed, and PHL showed
 * ten (or seven) default fields for one that never was.
 *
 * The seven a DatePeriod shows are FABRICATED from php's struct rather than
 * stored, so an object with no struct reads them as the zeroed one: `recurrences`
 * is 0 and `include_start_date` false, not the 1 and true a constructed period
 * ends up with. A subclass's own properties are the OBJECT's, not the struct's,
 * and still show either way. */
date_default_timezone_set('UTC');

class DtShapeIv extends DateInterval { public $mine = 7; public function __construct() {} }
class DtShapeDp extends DatePeriod { public $mine = 7; public function __construct() {} }

function dtshape_new($class) {
    return (new ReflectionClass($class))->newInstanceWithoutConstructor();
}
function dtshape_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-36s %s\n", $label, is_string($r) ? $r : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-36s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}
function dtshape_dump($label, $fn) {
    dtshape_show($label, function () use ($fn) {
        ob_start();
        var_dump($fn());
        return preg_replace('/#\d+ /', '#N ', trim(ob_get_clean()));
    });
}

/* --- what an unconstructed one shows --- */
$i = dtshape_new('DateInterval');
$p = dtshape_new('DatePeriod');
dtshape_dump('interval var_dump', fn() => $i);
dtshape_show('interval (array)', fn() => (array)$i);
dtshape_show('interval json', fn() => json_encode($i));
dtshape_show('interval print_r', fn() => trim(print_r($i, true)));
dtshape_show('interval var_export', fn() => var_export($i, true));
dtshape_dump('period var_dump', fn() => $p);
dtshape_show('period (array)', fn() => (array)$p);
dtshape_show('period json', fn() => json_encode($p));

/* a SUBCLASS's own property is the object's, and shows for both */
dtshape_dump('interval subclass var_dump', fn() => new DtShapeIv);
dtshape_show('interval subclass (array)', fn() => (array)new DtShapeIv);
dtshape_show('interval subclass json', fn() => json_encode(new DtShapeIv));
dtshape_dump('period subclass var_dump', fn() => new DtShapeDp);
dtshape_show('period subclass (array)', fn() => (array)new DtShapeDp);

/* --- a CONSTRUCTED one is unchanged --- */
dtshape_show('constructed interval (array)', fn() => (array)new DateInterval('P1D'));
dtshape_show('constructed interval json', fn() => json_encode(new DateInterval('P1D')));
dtshape_dump('diff interval var_dump', fn() => (new DateTime('@0'))->diff(new DateTime('@86400')));
dtshape_show('constructed period keys',
    fn() => array_keys((array)new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2)));
dtshape_show('constructed period recurrences',
    fn() => (new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2))->recurrences);
dtshape_show('period from an ISO string',
    fn() => (new DatePeriod('R2/1970-01-01T00:00:00Z/P1D'))->recurrences);

/* --- the seven a period fabricates, read off an object with no struct --- */
foreach (['start', 'current', 'end', 'interval', 'recurrences',
          'include_start_date', 'include_end_date'] as $k) {
    dtshape_show("unconstructed \$p->$k", fn() => $p->$k);
}
/* and off a real one, including the END-DATE form php answers 1 for */
$q = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), new DateTime('@172800'));
dtshape_show('end-date period recurrences', fn() => $q->recurrences);
dtshape_show('end-date period getRecurrences', fn() => $q->getRecurrences());
dtshape_show('end-date period include_start', fn() => $q->include_start_date);
dtshape_show('end-date period walks', fn() => count(iterator_to_array($q)));
dtshape_show('excluded-start period',
    fn() => (new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2,
        DatePeriod::EXCLUDE_START_DATE))->include_start_date);
dtshape_show('included-end period',
    fn() => (new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), new DateTime('@172800'),
        DatePeriod::INCLUDE_END_DATE))->include_end_date);
--EXPECT--
interval var_dump                    object(DateInterval)#N (0) {
}
interval (array)                     array (
)
interval json                        {}
interval print_r                     DateInterval Object
(
)
interval var_export                  \DateInterval::__set_state(array(
))
period var_dump                      object(DatePeriod)#N (0) {
}
period (array)                       array (
)
period json                          {}
interval subclass var_dump           object(DtShapeIv)#N (1) {
  ["mine"]=>
  int(7)
}
interval subclass (array)            array (
  'mine' => 7,
)
interval subclass json               {"mine":7}
period subclass var_dump             object(DtShapeDp)#N (1) {
  ["mine"]=>
  int(7)
}
period subclass (array)              array (
  'mine' => 7,
)
constructed interval (array)         array (
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
constructed interval json            {"y":0,"m":0,"d":1,"h":0,"i":0,"s":0,"f":0,"invert":0,"days":false,"from_string":false}
diff interval var_dump               object(DateInterval)#N (10) {
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
constructed period keys              array (
  0 => 'start',
  1 => 'current',
  2 => 'end',
  3 => 'interval',
  4 => 'recurrences',
  5 => 'include_start_date',
  6 => 'include_end_date',
)
constructed period recurrences       3
period from an ISO string            3
unconstructed $p->start              NULL
unconstructed $p->current            NULL
unconstructed $p->end                NULL
unconstructed $p->interval           NULL
unconstructed $p->recurrences        0
unconstructed $p->include_start_date false
unconstructed $p->include_end_date   false
end-date period recurrences          1
end-date period getRecurrences       NULL
end-date period include_start        true
end-date period walks                2
excluded-start period                false
included-end period                  true
