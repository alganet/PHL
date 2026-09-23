--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DateInterval, DatePeriod and the procedural date API are C, iterator included
--FILE--
<?php
function dpncShow($label, $fn) {
    echo $label, ': ';
    try {
        $r = $fn();
        echo is_string($r) ? $r : var_export($r, true);
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage();
    }
    echo "\n";
}

// Class constants of a C-declared class and interface.
echo 'consts: ', DateTime::ATOM, ' ', DateTimeInterface::RFC7231, ' ',
     DatePeriod::EXCLUDE_START_DATE, DatePeriod::INCLUDE_END_DATE, "\n";

// DateInterval: both constructors, in C.
$iv = new DateInterval('P1Y2M3WT4H5M6S');
echo "iso: {$iv->y} {$iv->m} {$iv->d} {$iv->h} {$iv->i} {$iv->s} ",
     var_export($iv->days, true), ' ', var_export($iv->f, true), "\n";
dpncShow('iso bad', fn() => new DateInterval('P1X'));
dpncShow('iso bare P', fn() => new DateInterval('P'));
dpncShow('iso trailing T', fn() => new DateInterval('P1DT'));
dpncShow('iso fraction', fn() => new DateInterval('PT1.5S'));
dpncShow('iso negative', fn() => new DateInterval('P-1D'));
$rel = DateInterval::createFromDateString('1 year + 3 months');
echo "rel: {$rel->y} {$rel->m}\n";
$rel2 = DateInterval::createFromDateString('2 weeks, 3 hours');
echo "rel2: {$rel2->d} {$rel2->h}\n";
dpncShow('rel empty', fn() => DateInterval::createFromDateString(''));
dpncShow('rel no unit', function(){ $i = DateInterval::createFromDateString('next monday');
                                    return "{$i->y} {$i->m} {$i->d}"; });
// An unknown token keeps its '%'; a trailing one is dropped.
echo 'fmt: ', (new DateInterval('P1Y2M3DT4H5M6S'))->format('%Y-%M-%D %H:%I:%S|%y %m %d|%R%r|%a|%q|%%|%'), "\n";

// DatePeriod, and the InternalIterator its getIterator() answers.
$start = new DateTime('2020-01-01 00:00:00', new DateTimeZone('UTC'));
$day = new DateInterval('P1D');
$end = new DateTime('2020-01-04 00:00:00', new DateTimeZone('UTC'));
$dump = function ($p) { $o = []; foreach ($p as $k => $d) { $o[] = $k . ':' . $d->format('m-d'); }
                        return implode(' ', $o); };
echo 'recur: ', $dump(new DatePeriod($start, $day, 2)), "\n";
echo 'end: ', $dump(new DatePeriod($start, $day, $end)), "\n";
echo 'excl start: ', $dump(new DatePeriod($start, $day, $end, DatePeriod::EXCLUDE_START_DATE)), "\n";
echo 'incl end: ', $dump(new DatePeriod($start, $day, $end, DatePeriod::INCLUDE_END_DATE)), "\n";
echo 'iso: ', $dump(new DatePeriod('R2/2020-01-01T00:00:00Z/P1D')), "\n";
$p = new DatePeriod($start, $day, 3);
echo 'accessors: ', get_class($p->getStartDate()), ' ', var_export($p->getEndDate(), true), ' ',
     get_class($p->getDateInterval()), ' ', var_export($p->getRecurrences(), true), ' ',
     var_export($p->recurrences, true), "\n";
echo 'end recurrences: ', var_export((new DatePeriod($start, $day, $end))->getRecurrences(), true), "\n";
// The two ISO entry points disagree on the class they build, and php means it.
echo 'iso classes: ', get_class((new DatePeriod('R1/2020-01-01T00:00:00Z/P1D'))->getStartDate()), ' ',
     get_class(DatePeriod::createFromISO8601String('R1/2020-01-01T00:00:00Z/P1D')->getStartDate()), "\n";
$it = $p->getIterator();
echo 'iterator: ', get_class($it), ' final=',
     var_export((new ReflectionClass('InternalIterator'))->isFinal(), true), "\n";
$o = [];
for ($it->rewind(); $it->valid(); $it->next()) { $o[] = $it->key() . ':' . $it->current()->format('m-d'); }
echo 'manual: ', implode(' ', $o), "\n";
echo 'fresh each call: ', var_export($p->getIterator() === $p->getIterator(), true),
     ' twice: ', count(iterator_to_array($p)), '/', count(iterator_to_array($p)), "\n";
dpncShow('iterator ctor', fn() => new InternalIterator());
dpncShow('period no interval', fn() => new DatePeriod($start));
dpncShow('period bad interval', fn() => new DatePeriod($start, 'P1D', 2));
dpncShow('period bad iso', fn() => new DatePeriod('R2/nope/P1D'));

// The procedural API: C functions with php's signatures, not forwards.
echo 'proc: ', date_format(date_create('2020-01-02 03:04:05'), 'c'), ' ',
     date_format(date_add(date_create('2020-01-02'), $day), 'Y-m-d'), ' ',
     date_diff(date_create('2020-01-01'), date_create('2020-03-05'))->format('%a'), ' ',
     timezone_name_get(date_timezone_get(date_create('2020-01-02'))), ' ',
     date_interval_format(date_interval_create_from_date_string('3 days'), '%d'), "\n";
echo 'proc false: ', var_export(date_create('zzz'), true), ' ',
     var_export(date_create_from_format('Y-m-d', 'zz'), true), "\n";
dpncShow('proc immutable', fn() => date_modify(date_create_immutable('@0'), '+1 day'));
dpncShow('proc wrong class', fn() => timezone_name_get(date_create('@0')));
dpncShow('proc arity', fn() => date_format(date_create('@0')));
$f = new ReflectionFunction('date_format');
echo 'proc reflection: ', $f->getNumberOfParameters(), '/', $f->getNumberOfRequiredParameters(),
     ' internal=', var_export($f->isInternal(), true), "\n";
?>
--EXPECT--
consts: Y-m-d\TH:i:sP D, d M Y H:i:s \G\M\T 12
iso: 1 2 21 4 5 6 false 0.0
iso bad: DateMalformedIntervalStringException: Unknown or bad format (P1X)
iso bare P: DateMalformedIntervalStringException: Unknown or bad format (P)
iso trailing T: DateMalformedIntervalStringException: Unknown or bad format (P1DT)
iso fraction: DateMalformedIntervalStringException: Unknown or bad format (PT1.5S)
iso negative: DateMalformedIntervalStringException: Unknown or bad format (P-1D)
rel: 1 3
rel2: 14 3
rel empty: DateMalformedIntervalStringException: Unknown or bad format () at position 0 ( ): Empty string
rel no unit: 0 0 0
fmt: 01-02-03 04:05:06|1 2 3|+|(unknown)|%q|%|
recur: 0:01-01 1:01-02 2:01-03
end: 0:01-01 1:01-02 2:01-03
excl start: 0:01-02 1:01-03
incl end: 0:01-01 1:01-02 2:01-03 3:01-04
iso: 0:01-01 1:01-02 2:01-03
accessors: DateTime NULL DateInterval 3 4
end recurrences: NULL
iso classes: DateTime DateTimeImmutable
iterator: InternalIterator final=true
manual: 0:01-01 1:01-02 2:01-03 3:01-04
fresh each call: false twice: 4/4
iterator ctor: Error: Call to private InternalIterator::__construct() from global scope
period no interval: TypeError: DatePeriod::__construct() accepts (DateTimeInterface, DateInterval, int [, int]), or (DateTimeInterface, DateInterval, DateTime [, int]), or (string [, int]) as arguments
period bad interval: TypeError: DatePeriod::__construct() accepts (DateTimeInterface, DateInterval, int [, int]), or (DateTimeInterface, DateInterval, DateTime [, int]), or (string [, int]) as arguments
period bad iso: DateMalformedPeriodStringException: Unknown or bad format (R2/nope/P1D)
proc: 2020-01-02T03:04:05+00:00 2020-01-03 64 UTC 3
proc false: false false
proc immutable: TypeError: date_modify(): Argument #1 ($object) must be of type DateTime, DateTimeImmutable given
proc wrong class: TypeError: timezone_name_get(): Argument #1 ($object) must be of type DateTimeZone, DateTime given
proc arity: ArgumentCountError: date_format() expects exactly 2 arguments, 1 given
proc reflection: 2/2 internal=true
--CLEAN--
<?php
unset($iv, $rel, $rel2, $start, $day, $end, $dump, $p, $it, $o, $f);
