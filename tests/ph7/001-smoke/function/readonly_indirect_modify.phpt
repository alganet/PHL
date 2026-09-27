--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A reference cannot reach a property no write may reach
--FILE--
<?php
/* php refuses a store to a readonly property, and it refuses just as loudly every
 * way of reaching one INDIRECTLY: the alias outlives the statement, so a write
 * through it would land on the property with the readonly latch — or, for a
 * native class, the write handler — no longer in the way. PHL screened the store
 * and nothing else, so a readonly property could be rewritten through a `=&`
 * bind, a by-reference parameter (a user function's or a builtin's), a
 * by-reference foreach, or a subscript write into the array it held.
 *
 * `Cannot indirectly modify readonly property C::$p` is php's sentence for its
 * own readonly flag, raised whatever the scope — from inside the declaring class
 * too. A native class whose handler refuses every write (DatePeriod's seven) gets
 * that handler's own sentence instead, the one a plain store to it gets.
 *
 * The screen runs BEFORE the shape of the subject is judged, which is visible on
 * the two ends: a by-reference foreach over an INT property is this Error rather
 * than the "must be of type array|object" warning, and an unset() reaching into
 * an OBJECT one is this Error rather than "Cannot use object of type C as
 * array". */
date_default_timezone_set('UTC');

class RoInd {
    public function __construct(
        public readonly array $a,
        public readonly int $n,
        public array $open,
    ) {}
    public function insideBind() { $r = &$this->n; return 'bound'; }
    public function insideArg() { roind_take($this->n); return 'passed'; }
}
function roind_take(&$x) { $x = 111; }
function roind_take_arr(&$x) { $x = [9]; }
function roind_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-28s %s\n", $label, var_export($r, true));
    } catch (Throwable $e) {
        printf("%-28s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}

$o = new RoInd([3, 1, 2], 5, [3, 1, 2]);
roind_show('bind', function () use ($o) { $r = &$o->n; return 'bound'; });
roind_show('user by-ref', function () use ($o) { roind_take($o->n); return 'passed'; });
roind_show('sort', function () use ($o) { sort($o->a); return 'sorted'; });
roind_show('array_push', function () use ($o) { array_push($o->a, 9); return 'pushed'; });
roind_show('preg_match out', function () use ($o) { preg_match('/a/', 'a', $o->n); return 'matched'; });
roind_show('foreach by-ref', function () use ($o) { foreach ($o->a as &$v) { $v = 9; } return 'looped'; });
roind_show('foreach by-ref on int', function () use ($o) { foreach ($o->n as &$v) { } return 'looped'; });
roind_show('subscript write', function () use ($o) { $o->a[0] = 9; return 'written'; });
roind_show('append', function () use ($o) { $o->a[] = 9; return 'appended'; });
roind_show('unset element', function () use ($o) { unset($o->a[0]); return 'unset'; });
roind_show('inside the class, bind', fn () => $o->insideBind());
roind_show('inside the class, arg', fn () => $o->insideArg());

/* Nothing landed. */
roind_show('n', fn () => $o->n);
roind_show('a', fn () => implode(',', $o->a));

/* The same property WITHOUT readonly takes every one of them, so the screen is
 * the flag's and not the property machinery's. */
roind_show('open sort', function () use ($o) { sort($o->open); return implode(',', $o->open); });
roind_show('open by-ref', function () use ($o) { roind_take_arr($o->open); return implode(',', $o->open); });
roind_show('open append', function () use ($o) { $o->open = [1]; $o->open[] = 2; return implode(',', $o->open); });
roind_show('open foreach', function () use ($o) {
    foreach ($o->open as &$v) { $v = 7; }
    unset($v);
    return implode(',', $o->open);
});

/* A native class whose handler refuses every write answers with the handler's
 * own sentence, at every one of the same doors. */
$p = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2);
roind_show('period bind', function () use ($p) { $r = &$p->recurrences; return 'bound'; });
roind_show('period by-ref', function () use ($p) { roind_take($p->recurrences); return 'passed'; });
roind_show('period sort', function () use ($p) { sort($p->interval); return 'sorted'; });
roind_show('period foreach', function () use ($p) { foreach ($p->recurrences as &$v) { } return 'looped'; });
roind_show('period unset element', function () use ($p) { unset($p->interval[0]); return 'unset'; });
roind_show('period recurrences', fn () => $p->recurrences);

/* An ordinary variable and an ordinary array are untouched by any of it. */
roind_show('plain array', function () {
    $a = [3, 1, 2];
    sort($a);
    foreach ($a as &$v) { $v *= 2; }
    unset($v);
    unset($a[0]);
    return implode(',', $a);
});
?>
--EXPECT--
bind                         Error: Cannot indirectly modify readonly property RoInd::$n
user by-ref                  Error: Cannot indirectly modify readonly property RoInd::$n
sort                         Error: Cannot indirectly modify readonly property RoInd::$a
array_push                   Error: Cannot indirectly modify readonly property RoInd::$a
preg_match out               Error: Cannot indirectly modify readonly property RoInd::$n
foreach by-ref               Error: Cannot indirectly modify readonly property RoInd::$a
foreach by-ref on int        Error: Cannot indirectly modify readonly property RoInd::$n
subscript write              Error: Cannot indirectly modify readonly property RoInd::$a
append                       Error: Cannot indirectly modify readonly property RoInd::$a
unset element                Error: Cannot indirectly modify readonly property RoInd::$a
inside the class, bind       Error: Cannot indirectly modify readonly property RoInd::$n
inside the class, arg        Error: Cannot indirectly modify readonly property RoInd::$n
n                            5
a                            '3,1,2'
open sort                    '1,2,3'
open by-ref                  '9'
open append                  '1,2'
open foreach                 '7,7'
period bind                  Error: Cannot modify readonly property DatePeriod::$recurrences
period by-ref                Error: Cannot modify readonly property DatePeriod::$recurrences
period sort                  Error: Cannot modify readonly property DatePeriod::$interval
period foreach               Error: Cannot modify readonly property DatePeriod::$recurrences
period unset element         Error: Cannot modify readonly property DatePeriod::$interval
period recurrences           3
plain array                  '4,6'
