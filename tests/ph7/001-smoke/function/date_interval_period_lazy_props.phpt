--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unconstructed interval or period has no property table at all
--FILE--
<?php
/* php keeps DateInterval's ten and DatePeriod's seven in a C struct its
 * constructor allocates, and writes the property TABLE from that struct — so an
 * object nobody constructed has no such property at all. The presentation
 * surfaces already showed nothing for one; every other door still answered ten
 * (or seven) declared defaults, which is what a read, isset(), get_object_vars()
 * and a property foreach all walk.
 *
 * The two classes differ in what a READ of a still-absent name answers, and it is
 * php's split between its two handlers: DatePeriod declares its seven (Reflection
 * lists them) and reads them from the ZEROED struct — null/0/false, in silence —
 * while DateInterval declares nothing at all, so its ten are an ordinary
 * `Undefined property` warning until the constructor runs. */
date_default_timezone_set('UTC');

class DtLazyIv extends DateInterval { public $mine = 'M'; public function __construct() {} }
class DtLazyDp extends DatePeriod { public $mine = 'M'; public function __construct() {} }

function dtlazy_new($class) {
    return (new ReflectionClass($class))->newInstanceWithoutConstructor();
}
function dtlazy_show($label, $fn) {
    set_error_handler(function ($no, $str) {
        if (error_reporting() === 0) { return false; }
        echo '    warn: ', $str, "\n";
        return true;
    });
    try {
        $r = $fn();
        printf("%-34s %s\n", $label, is_string($r) ? $r : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-34s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
    restore_error_handler();
}
function dtlazy_names($o) {
    return implode(',', array_map(fn ($p) => $p->getName(), (new ReflectionObject($o))->getProperties()));
}

/* --- the ten a DateInterval has not got yet --- */
$i = dtlazy_new('DateInterval');
dtlazy_show('iv read y', fn () => $i->y);
dtlazy_show('iv read days', fn () => $i->days);
dtlazy_show('iv isset y', fn () => isset($i->y));
dtlazy_show('iv empty y', fn () => empty($i->y));
dtlazy_show('iv coalesce y', fn () => $i->y ?? 'DEFAULT');
dtlazy_show('iv get_object_vars', fn () => get_object_vars($i));
dtlazy_show('iv (array)', fn () => (array) $i);
dtlazy_show('iv json_encode', fn () => json_encode($i));
dtlazy_show('iv foreach', function () use ($i) { $k = []; foreach ($i as $n => $v) { $k[] = $n; } return $k; });
dtlazy_show('iv ReflectionObject', fn () => dtlazy_names($i));
dtlazy_show('iv clone vars', fn () => get_object_vars(clone $i));

/* --- the seven a DatePeriod declares but does not hold --- */
$p = dtlazy_new('DatePeriod');
dtlazy_show('dp read start', fn () => $p->start);
dtlazy_show('dp read recurrences', fn () => $p->recurrences);
dtlazy_show('dp read include_start', fn () => $p->include_start_date);
dtlazy_show('dp read nope', fn () => $p->nope);
dtlazy_show('dp isset recurrences', fn () => isset($p->recurrences));
dtlazy_show('dp coalesce recurrences', fn () => $p->recurrences ?? 'DEFAULT');
dtlazy_show('dp coalesce start', fn () => $p->start ?? 'DEFAULT');
dtlazy_show('dp get_object_vars', fn () => get_object_vars($p));
dtlazy_show('dp foreach props', function () use ($p) { $k = []; foreach ($p as $n => $v) { $k[] = $n; } return $k; });
dtlazy_show('dp ReflectionObject', fn () => dtlazy_names($p));
dtlazy_show('dp getStartDate', fn () => $p->getStartDate());
dtlazy_show('dp getEndDate', fn () => $p->getEndDate());

/* --- a SUBCLASS's own property is the object's, and is all there is --- */
$si = dtlazy_new('DtLazyIv');
$sp = dtlazy_new('DtLazyDp');
dtlazy_show('sub iv vars', fn () => get_object_vars($si));
dtlazy_show('sub iv reflection', fn () => dtlazy_names($si));
dtlazy_show('sub dp vars', fn () => get_object_vars($sp));
dtlazy_show('sub dp reflection', fn () => dtlazy_names($sp));

/* --- the constructor installs the set, in php's order: the object's own
 * properties keep their POSITION and the struct's are appended behind them --- */
$ci = new DateInterval('P1Y2DT3H');
dtlazy_show('built iv vars', fn () => implode(',', array_keys(get_object_vars($ci))));
dtlazy_show('built iv reflection', fn () => dtlazy_names($ci));
$csi = new class ('P1D') extends DateInterval { public $mine = 'M'; };
dtlazy_show('built sub iv vars', fn () => implode(',', array_keys(get_object_vars($csi))));
$cp = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2);
dtlazy_show('built dp vars', fn () => implode(',', array_keys(get_object_vars($cp))));
dtlazy_show('built dp recurrences', fn () => $cp->recurrences);
dtlazy_show('built dp walk', function () use ($cp) {
    $o = [];
    foreach ($cp as $d) { $o[] = $d->format('Y-m-d'); }
    return implode(' ', $o);
});
dtlazy_show('built dp clone walk', function () use ($cp) {
    $o = [];
    foreach (clone $cp as $d) { $o[] = $d->format('Y-m-d'); }
    return implode(' ', $o);
});

/* --- the C factories build a complete object without a constructor --- */
$diff = (new DateTime('@0'))->diff(new DateTime('@100000'));
dtlazy_show('diff vars', fn () => implode(',', array_keys(get_object_vars($diff))));
dtlazy_show('diff days', fn () => $diff->days);
$iso = DatePeriod::createFromISO8601String('R2/2020-01-01T00:00:00Z/P1D');
dtlazy_show('iso vars', fn () => implode(',', array_keys(get_object_vars($iso))));
$back = unserialize(serialize($ci));
dtlazy_show('unserialize vars', fn () => implode(',', array_keys(get_object_vars($back))));
dtlazy_show('unserialize d', fn () => $back->d);

/* --- once installed, the ordinary declared-property rules are back: a write
 * lands, and unset() on a handler-backed property is php's silent no-op --- */
$ci->d = 9;
dtlazy_show('write d', fn () => $ci->d);
unset($ci->y);
dtlazy_show('unset y then read', fn () => $ci->y);
dtlazy_show('unset y then isset', fn () => isset($ci->y));
?>
--EXPECT--
    warn: Undefined property: DateInterval::$y
iv read y                          NULL
    warn: Undefined property: DateInterval::$days
iv read days                       NULL
iv isset y                         false
iv empty y                         true
iv coalesce y                      DEFAULT
iv get_object_vars                 array (
)
iv (array)                         array (
)
iv json_encode                     {}
iv foreach                         array (
)
iv ReflectionObject                
iv clone vars                      array (
)
dp read start                      NULL
dp read recurrences                0
dp read include_start              false
    warn: Undefined property: DatePeriod::$nope
dp read nope                       NULL
dp isset recurrences               false
dp coalesce recurrences            0
dp coalesce start                  DEFAULT
dp get_object_vars                 array (
)
dp foreach props                   DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
dp ReflectionObject                start,current,end,interval,recurrences,include_start_date,include_end_date
dp getStartDate                    DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
dp getEndDate                      NULL
sub iv vars                        array (
  'mine' => 'M',
)
sub iv reflection                  mine
sub dp vars                        array (
  'mine' => 'M',
)
sub dp reflection                  mine,start,current,end,interval,recurrences,include_start_date,include_end_date
built iv vars                      y,m,d,h,i,s,f,invert,days,from_string
built iv reflection                y,m,d,h,i,s,f,invert,days,from_string
built sub iv vars                  mine,y,m,d,h,i,s,f,invert,days,from_string
built dp vars                      start,current,end,interval,recurrences,include_start_date,include_end_date
built dp recurrences               3
built dp walk                      1970-01-01 1970-01-02 1970-01-03
built dp clone walk                1970-01-01 1970-01-02 1970-01-03
diff vars                          y,m,d,h,i,s,f,invert,days,from_string
diff days                          1
iso vars                           start,current,end,interval,recurrences,include_start_date,include_end_date
unserialize vars                   y,m,d,h,i,s,f,invert,days,from_string
unserialize d                      2
write d                            9
unset y then read                  1
unset y then isset                 true
