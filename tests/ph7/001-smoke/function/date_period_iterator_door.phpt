--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DatePeriod's walk: its cursor, its refusal, and a subclass's
--FILE--
<?php
/* Three things about the iterator a native aggregate answers, all of them the
 * DatePeriod walk's:
 *
 * `$period->current` IS that walk's cursor — the date the iterator sits on, and,
 * once the walk is over, the one PAST the end: the date that failed the test.
 * php writes it from the iterator's METHODS rather than from the walk, so a
 * getIterator() nobody has touched yet leaves it where the last walk left it and
 * the first valid()/current()/key()/rewind()/next() moves it. PHL left it null
 * forever, so a program reading the period mid-walk saw nothing.
 *
 * php refuses the WALK of an unconstructed period and not the door to it:
 * getIterator() hands back a real InternalIterator there and the DateObjectError
 * arrives at the first rewind(). Its sentence is the iterator's own, and it is
 * the one message in this family that never names a subclass.
 *
 * And a SUBCLASS inherits the walk the way it inherits the getIterator() that
 * reaches it: `class P extends DatePeriod {}` answered a real InternalIterator
 * that yielded NOTHING, so every foreach over one was silently empty. */
date_default_timezone_set('UTC');

class DtWalkP extends DatePeriod {}
class DtWalkP2 extends DtWalkP {}

function dtwalk_cur($p) { return $p->current?->format('Y-m-d') ?? 'NULL'; }
function dtwalk_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-30s %s\n", $label, is_string($r) ? $r : var_export($r, true));
    } catch (Throwable $e) {
        printf("%-30s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}

/* --- the cursor --- */
$p = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2);
dtwalk_show('fresh', fn () => dtwalk_cur($p));
$i1 = $p->getIterator();
dtwalk_show('after getIterator', fn () => dtwalk_cur($p));
dtwalk_show('after valid', function () use ($p, $i1) { $i1->valid(); return dtwalk_cur($p); });
dtwalk_show('after next', function () use ($p, $i1) { $i1->next(); return dtwalk_cur($p); });
$i2 = $p->getIterator();
dtwalk_show('after 2nd getIterator', fn () => dtwalk_cur($p));
dtwalk_show('after rewind', function () use ($p, $i2) { $i2->rewind(); return dtwalk_cur($p); });
dtwalk_show('during the walk', function () use ($p) {
    $o = [];
    foreach ($p as $d) { $o[] = dtwalk_cur($p); }
    return implode(' ', $o);
});
dtwalk_show('past the end', fn () => dtwalk_cur($p));

/* An END-bounded period stops on the date that met the end, and that is what the
 * cursor is left holding. */
$e = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), new DateTime('@172800'));
dtwalk_show('end-bounded walk', function () use ($e) {
    $o = [];
    foreach ($e as $d) { $o[] = dtwalk_cur($e); }
    return implode(' ', $o);
});
dtwalk_show('end-bounded after', fn () => dtwalk_cur($e));

/* An EXCLUDED start date is stepped over, and the cursor never shows it. */
$x = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2, DatePeriod::EXCLUDE_START_DATE);
dtwalk_show('excluded start walk', function () use ($x) {
    $o = [];
    foreach ($x as $d) { $o[] = dtwalk_cur($x); }
    return implode(' ', $o);
});

/* --- the refusal, at the walk rather than the door --- */
$u = (new ReflectionClass('DatePeriod'))->newInstanceWithoutConstructor();
$su = (new ReflectionClass('DtWalkP'))->newInstanceWithoutConstructor();
$su2 = (new ReflectionClass('DtWalkP2'))->newInstanceWithoutConstructor();
dtwalk_show('unbuilt getIterator', fn () => get_class($u->getIterator()));
dtwalk_show('unbuilt rewind', function () use ($u) { $u->getIterator()->rewind(); return 'rewound'; });
dtwalk_show('unbuilt foreach', function () use ($u) { foreach ($u as $d) { } return 'walked'; });
dtwalk_show('sub getIterator', fn () => get_class($su->getIterator()));
dtwalk_show('sub foreach', function () use ($su) { foreach ($su as $d) { } return 'walked'; });
dtwalk_show('sub-sub foreach', function () use ($su2) { foreach ($su2 as $d) { } return 'walked'; });
dtwalk_show('sub iterator_to_array', fn () => iterator_to_array($su));
/* the method door still names the object's own class, as every other one does */
dtwalk_show('sub getStartDate', fn () => $su->getStartDate());

/* --- a subclass really walks --- */
$sp = new DtWalkP(new DateTime('@0'), new DateInterval('P1D'), 2);
dtwalk_show('sub walk', function () use ($sp) {
    $o = [];
    foreach ($sp as $d) { $o[] = $d->format('Y-m-d'); }
    return implode(' ', $o);
});
dtwalk_show('sub iterator class', fn () => get_class($sp->getIterator()));
dtwalk_show('sub to array', fn () => count(iterator_to_array($sp)));
dtwalk_show('sub cursor', fn () => dtwalk_cur($sp));
$sp2 = new DtWalkP2(new DateTime('@0'), new DateInterval('P1D'), 1);
dtwalk_show('sub-sub walk', function () use ($sp2) {
    $o = [];
    foreach ($sp2 as $d) { $o[] = $d->format('Y-m-d'); }
    return implode(' ', $o);
});
?>
--EXPECT--
fresh                          NULL
after getIterator              NULL
after valid                    1970-01-01
after next                     1970-01-02
after 2nd getIterator          1970-01-02
after rewind                   1970-01-01
during the walk                1970-01-01 1970-01-02 1970-01-03
past the end                   1970-01-04
end-bounded walk               1970-01-01 1970-01-02
end-bounded after              1970-01-03
excluded start walk            1970-01-02 1970-01-03
unbuilt getIterator            InternalIterator
unbuilt rewind                 DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
unbuilt foreach                DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
sub getIterator                InternalIterator
sub foreach                    DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
sub-sub foreach                DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
sub iterator_to_array          DateObjectError: Object of type DatePeriod has not been correctly initialized by calling parent::__construct() in its constructor
sub getStartDate               DateObjectError: Object of type DtWalkP (inheriting DatePeriod) has not been correctly initialized by calling parent::__construct() in its constructor
sub walk                       1970-01-01 1970-01-02 1970-01-03
sub iterator class             InternalIterator
sub to array                   3
sub cursor                     1970-01-04
sub-sub walk                   1970-01-01 1970-01-02
