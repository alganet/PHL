--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every write to a DatePeriod's seven properties is refused
--FILE--
<?php
/* php's write_property handler for DatePeriod refuses OUTRIGHT: the seven it
 * shows are a view of its C struct and a script may only read them. PHL kept real
 * slots a script could write, so a period could be driven to a shape no
 * constructor would have built — `$p->interval = null` left one iterating on
 * nothing, `$p->recurrences = 99` invented recurrences past the range the
 * constructor screens, and `unset($p->start)` removed the start date outright.
 *
 * Every write FORM is refused, not just `=`: `++`, a by-reference bind (php
 * refuses the bind itself, because the alias would let a later write through it
 * reach the struct), a destructuring target, and a write to an object that was
 * never constructed. The two sentences disagree about which class to name, and a
 * subclass shows it: the write says the DECLARING class and the unset says the
 * object's own. Neither is the `readonly` FLAG — Reflection reports
 * isReadOnly() false for all seven in both engines. */
date_default_timezone_set('UTC');

class DtRoDp extends DatePeriod { public $mine = 'M'; }

function dtro_show($label, $fn) {
    try {
        $r = $fn();
        printf("%-30s %s\n", $label, var_export($r, true));
    } catch (Throwable $e) {
        printf("%-30s %s: %s\n", $label, get_class($e), $e->getMessage());
    }
}

$p = new DatePeriod(new DateTime('@0'), new DateInterval('P1D'), 2);
foreach (['start', 'current', 'end', 'interval', 'recurrences', 'include_start_date', 'include_end_date'] as $n) {
    dtro_show("write $n", function () use ($p, $n) { $p->$n = null; return 'written'; });
    dtro_show("unset $n", function () use ($p, $n) { unset($p->$n); return 'unset'; });
}
dtro_show('increment', function () use ($p) { $p->recurrences++; return 'incremented'; });
dtro_show('decrement', function () use ($p) { $p->include_end_date--; return 'decremented'; });
dtro_show('compound', function () use ($p) { $p->recurrences += 2; return 'added'; });
dtro_show('by-reference bind', function () use ($p) { $r = &$p->recurrences; return 'bound'; });
dtro_show('destructure', function () use ($p) { [$p->recurrences] = [9]; return 'destructured'; });
dtro_show('coalesce assign', function () use ($p) { $p->end ??= 1; return 'assigned'; });

/* Nothing landed: the period is what its constructor made. */
dtro_show('recurrences', fn () => $p->recurrences);
dtro_show('walk', function () use ($p) {
    $o = [];
    foreach ($p as $d) { $o[] = $d->format('Y-m-d'); }
    return implode(' ', $o);
});

/* An object with no struct refuses the same way — php's handler does not consult
 * one — where a missing property would otherwise be a silent no-op. */
$q = (new ReflectionClass('DatePeriod'))->newInstanceWithoutConstructor();
dtro_show('unbuilt write', function () use ($q) { $q->start = 1; return 'written'; });
dtro_show('unbuilt unset', function () use ($q) { unset($q->start); return 'unset'; });
dtro_show('unbuilt vars', fn () => get_object_vars($q));

/* A SUBCLASS: the write names DatePeriod, the unset names the subclass, and the
 * subclass's OWN property is an ordinary one that takes both. */
$s = new DtRoDp(new DateTime('@0'), new DateInterval('P1D'), 2);
dtro_show('sub write', function () use ($s) { $s->interval = null; return 'written'; });
dtro_show('sub unset', function () use ($s) { unset($s->interval); return 'unset'; });
dtro_show('sub own write', function () use ($s) { $s->mine = 'W'; return $s->mine; });
dtro_show('sub own unset', function () use ($s) { unset($s->mine); return isset($s->mine); });

/* Reflection still reports them as not readonly, as php does. */
$r = new ReflectionClass('DatePeriod');
dtro_show('reflection readonly', fn () => implode(',', array_map(
    fn ($x) => $x->getName() . '=' . var_export($x->isReadOnly(), true), $r->getProperties())));
?>
--EXPECT--
write start                    Error: Cannot modify readonly property DatePeriod::$start
unset start                    Error: Cannot unset DatePeriod::$start
write current                  Error: Cannot modify readonly property DatePeriod::$current
unset current                  Error: Cannot unset DatePeriod::$current
write end                      Error: Cannot modify readonly property DatePeriod::$end
unset end                      Error: Cannot unset DatePeriod::$end
write interval                 Error: Cannot modify readonly property DatePeriod::$interval
unset interval                 Error: Cannot unset DatePeriod::$interval
write recurrences              Error: Cannot modify readonly property DatePeriod::$recurrences
unset recurrences              Error: Cannot unset DatePeriod::$recurrences
write include_start_date       Error: Cannot modify readonly property DatePeriod::$include_start_date
unset include_start_date       Error: Cannot unset DatePeriod::$include_start_date
write include_end_date         Error: Cannot modify readonly property DatePeriod::$include_end_date
unset include_end_date         Error: Cannot unset DatePeriod::$include_end_date
increment                      Error: Cannot modify readonly property DatePeriod::$recurrences
decrement                      Error: Cannot modify readonly property DatePeriod::$include_end_date
compound                       Error: Cannot modify readonly property DatePeriod::$recurrences
by-reference bind              Error: Cannot modify readonly property DatePeriod::$recurrences
destructure                    Error: Cannot modify readonly property DatePeriod::$recurrences
coalesce assign                Error: Cannot modify readonly property DatePeriod::$end
recurrences                    3
walk                           '1970-01-01 1970-01-02 1970-01-03'
unbuilt write                  Error: Cannot modify readonly property DatePeriod::$start
unbuilt unset                  Error: Cannot unset DatePeriod::$start
unbuilt vars                   array (
)
sub write                      Error: Cannot modify readonly property DatePeriod::$interval
sub unset                      Error: Cannot unset DtRoDp::$interval
sub own write                  'W'
sub own unset                  false
reflection readonly            'start=false,current=false,end=false,interval=false,recurrences=false,include_start_date=false,include_end_date=false'
