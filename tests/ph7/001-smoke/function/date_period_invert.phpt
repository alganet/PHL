--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DatePeriod walks forward whatever the interval's invert says
--FILE--
<?php
/* php's period walk reads the interval's FIELDS and not its `invert` flag, so a
 * period built on an interval a diff() answered -- or on a hand-set
 * `$iv->invert = 1` -- still runs FORWARD, while a negative FIELD really does
 * step backward. PHL honoured the flag, so such a period walked the wrong way,
 * and with an END date rather than a recurrence count it walked AWAY from that
 * end with nothing to stop it. */
date_default_timezone_set('UTC');
$inv = new DateInterval('P1D');
$inv->invert = 1;
foreach (new DatePeriod(new DateTime('2020-01-01'), $inv, 2) as $x) {
    echo 'invert=1: ', $x->format('Y-m-d'), "\n";
}
foreach (new DatePeriod(new DateTime('2020-01-01'), $inv, new DateTime('2020-01-04')) as $x) {
    echo 'to-end:   ', $x->format('Y-m-d'), "\n";
}
/* a diff()'s own inverted interval is the everyday way to hold one */
$back = (new DateTime('2020-01-05'))->diff(new DateTime('2020-01-01'));
var_dump($back->invert, $back->d);
foreach (new DatePeriod(new DateTime('2020-01-01'), $back, 2) as $x) {
    echo 'from-diff: ', $x->format('Y-m-d'), "\n";
}
/* ...while a NEGATIVE field steps backward, invert or not */
$neg = DateInterval::createFromDateString('-1 day');
var_dump($neg->d, $neg->invert);
foreach (new DatePeriod(new DateTime('2020-01-01'), $neg, 2) as $x) {
    echo 'negative: ', $x->format('Y-m-d'), "\n";
}
/* add()/sub() are the other half of php's split: THEY honour the flag */
$d = new DateTime('2020-01-01');
echo 'add: ', $d->add($inv)->format('Y-m-d'), "\n";
echo 'sub: ', (new DateTime('2020-01-01'))->sub($inv)->format('Y-m-d'), "\n";
?>
--EXPECT--
invert=1: 2020-01-01
invert=1: 2020-01-02
invert=1: 2020-01-03
to-end:   2020-01-01
to-end:   2020-01-02
to-end:   2020-01-03
int(1)
int(4)
from-diff: 2020-01-01
from-diff: 2020-01-05
from-diff: 2020-01-09
int(-1)
int(0)
negative: 2020-01-01
negative: 2019-12-31
negative: 2019-12-30
add: 2019-12-31
sub: 2020-01-02
--CLEAN--
<?php
/* the smoke corpus runs in ONE interpreter: leave no globals behind */
unset($inv, $x, $back, $neg, $d);
