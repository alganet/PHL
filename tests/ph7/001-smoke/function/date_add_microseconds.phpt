--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
add() and sub() move the clock by an interval's microseconds
--FILE--
<?php
/* php's `f` is a signed count of a second's fractions that moves the clock like
 * every other field of the interval: an interval carrying f = 2.5 and nothing
 * else moves it two and a half seconds. PHL ignored `f` at all four sites that
 * apply an interval -- add(), sub(), their procedural aliases and the
 * DatePeriod walk -- so adding one moved the clock by NOTHING, and a period
 * over a sub-second interval answered the start date forever. */
date_default_timezone_set('UTC');
$mk = function ($f, $spec = 'PT0S', $inv = 0) {
    $iv = new DateInterval($spec);
    $iv->f = $f;
    $iv->invert = $inv;
    return $iv;
};
foreach ([[0.5, 'PT0S', 0], [-0.5, 'PT0S', 0], [2.5, 'PT0S', 0], [0.75, 'PT1S', 0],
          [0.5, 'PT0S', 1], [-0.5, 'PT0S', 1], [0.25, 'PT0S', 0], [0.5, 'P1M', 0]] as [$f, $spec, $inv]) {
    $a = new DateTime('2020-01-31 00:00:10.100000');
    $b = new DateTimeImmutable('2020-01-31 00:00:10.100000');
    $iv = $mk($f, $spec, $inv);
    $a->add($iv);
    printf("f=%-5s %-5s inv=%d  add=%s  sub=%s  imm=%s\n", $f, $spec, $inv,
        $a->format('Y-m-d H:i:s.u'),
        (new DateTime('2020-01-31 00:00:10.100000'))->sub($iv)->format('Y-m-d H:i:s.u'),
        $b->add($iv)->format('Y-m-d H:i:s.u'));
}
/* the carry into the second is ordinary floor division, so a sub() past it
 * borrows */
$c = new DateTime('2020-01-01 00:00:10.900000'); $c->add($mk(0.2));
echo 'carry:  ', $c->format('Y-m-d H:i:s.u'), "\n";
$c = new DateTime('2020-01-01 00:00:10.100000'); $c->sub($mk(0.2));
echo 'borrow: ', $c->format('Y-m-d H:i:s.u'), "\n";
/* the procedural spelling is the same body */
$d = new DateTime('2020-01-01 00:00:00.000000');
date_add($d, $mk(1.25));
echo 'date_add: ', $d->format('Y-m-d H:i:s.u'), "\n";
date_sub($d, $mk(0.25));
echo 'date_sub: ', $d->format('Y-m-d H:i:s.u'), "\n";
/* ...and so is the period walk, which stood still over a sub-second interval */
foreach (new DatePeriod(new DateTime('2020-01-01 00:00:00.000000'), $mk(0.5), 3) as $x) {
    echo 'period: ', $x->format('Y-m-d H:i:s.u'), "\n";
}
?>
--EXPECT--
f=0.5   PT0S  inv=0  add=2020-01-31 00:00:10.600000  sub=2020-01-31 00:00:09.600000  imm=2020-01-31 00:00:10.600000
f=-0.5  PT0S  inv=0  add=2020-01-31 00:00:09.600000  sub=2020-01-31 00:00:10.600000  imm=2020-01-31 00:00:09.600000
f=2.5   PT0S  inv=0  add=2020-01-31 00:00:12.600000  sub=2020-01-31 00:00:07.600000  imm=2020-01-31 00:00:12.600000
f=0.75  PT1S  inv=0  add=2020-01-31 00:00:11.850000  sub=2020-01-31 00:00:08.350000  imm=2020-01-31 00:00:11.850000
f=0.5   PT0S  inv=1  add=2020-01-31 00:00:09.600000  sub=2020-01-31 00:00:10.600000  imm=2020-01-31 00:00:09.600000
f=-0.5  PT0S  inv=1  add=2020-01-31 00:00:10.600000  sub=2020-01-31 00:00:09.600000  imm=2020-01-31 00:00:10.600000
f=0.25  PT0S  inv=0  add=2020-01-31 00:00:10.350000  sub=2020-01-31 00:00:09.850000  imm=2020-01-31 00:00:10.350000
f=0.5   P1M   inv=0  add=2020-03-02 00:00:10.600000  sub=2019-12-31 00:00:09.600000  imm=2020-03-02 00:00:10.600000
carry:  2020-01-01 00:00:11.100000
borrow: 2020-01-01 00:00:09.900000
date_add: 2020-01-01 00:00:01.250000
date_sub: 2020-01-01 00:00:01.000000
period: 2020-01-01 00:00:00.000000
period: 2020-01-01 00:00:00.500000
period: 2020-01-01 00:00:01.000000
period: 2020-01-01 00:00:01.500000
--CLEAN--
<?php
/* the smoke corpus runs in ONE interpreter: leave no globals behind */
unset($mk, $f, $spec, $inv, $a, $b, $iv, $c, $d, $x);
