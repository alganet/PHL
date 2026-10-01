--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
diff() has two rules, and the same zone spelled twice is two zones
--FILE--
<?php
/* diff() has TWO rules and php picks between them on whether the two dates are
 * in the same zone -- where "same" is a byte-exact identifier comparison, so
 * `America/New_York` and `america/new_york` are one place and two zones, and so
 * are `America/New_York` and `US/Eastern`.
 *
 * Same zone: each date is read on its own clock, and January to August in New
 * York is seven months exactly, with the hour daylight saving took nowhere in
 * the answer. Different zones: both are read on the EARLIER one's offset, and
 * the same pair spelled the other way is seven months LESS AN HOUR. */
date_default_timezone_set('UTC');

$pairs = [
    ['America/New_York', 'America/New_York'],
    ['America/New_York', 'america/new_york'],
    ['America/New_York', 'US/Eastern'],
    ['America/New_York', 'Europe/Paris'],
    ['America/New_York', 'UTC'],
    ['America/New_York', 'EST'],
    ['Europe/Paris', 'Europe/Paris'],
    ['Australia/Sydney', 'Australia/Sydney'],
    ['+05:00', '+05:00'],
];
foreach ($pairs as [$z1, $z2]) {
    $a = new DateTime('2010-01-01 00:00:00', new DateTimeZone($z1));
    $b = new DateTime('2010-08-01 00:00:00', new DateTimeZone($z2));
    printf("%-18s %-18s fwd %s days=%-4d inv=%d\n", $z1, $z2,
        $a->diff($b)->format('%R%y-%m-%d %h:%i:%s'), $a->diff($b)->days, $a->diff($b)->invert);
    printf("%-18s %-18s rev %s days=%-4d inv=%d\n", $z1, $z2,
        $b->diff($a)->format('%R%y-%m-%d %h:%i:%s'), $b->diff($a)->days, $b->diff($a)->invert);
}

$z = new DateTimeZone('America/New_York');
foreach ([['2010-03-13 12:00:00', '2010-03-14 12:00:00'],
          ['2010-11-06 12:00:00', '2010-11-07 12:00:00']] as [$s1, $s2]) {
    $iv = (new DateTime($s1, $z))->diff(new DateTime($s2, $z));
    printf("across %s -> %s : %s days=%d\n", $s1, $s2, $iv->format('%R%y-%m-%d %h:%i:%s'), $iv->days);
}
?>
--EXPECT--
America/New_York   America/New_York   fwd +0-7-0 0:0:0 days=212  inv=0
America/New_York   America/New_York   rev -0-7-0 0:0:0 days=212  inv=1
America/New_York   america/new_york   fwd +0-6-30 23:0:0 days=211  inv=0
America/New_York   america/new_york   rev -0-6-30 23:0:0 days=211  inv=1
America/New_York   US/Eastern         fwd +0-6-30 23:0:0 days=211  inv=0
America/New_York   US/Eastern         rev -0-6-30 23:0:0 days=211  inv=1
America/New_York   Europe/Paris       fwd +0-6-30 17:0:0 days=211  inv=0
America/New_York   Europe/Paris       rev -0-6-30 17:0:0 days=211  inv=1
America/New_York   UTC                fwd +0-6-30 19:0:0 days=211  inv=0
America/New_York   UTC                rev -0-6-30 19:0:0 days=211  inv=1
America/New_York   EST                fwd +0-7-0 0:0:0 days=212  inv=0
America/New_York   EST                rev -0-7-0 0:0:0 days=212  inv=1
Europe/Paris       Europe/Paris       fwd +0-7-0 0:0:0 days=212  inv=0
Europe/Paris       Europe/Paris       rev -0-7-0 0:0:0 days=212  inv=1
Australia/Sydney   Australia/Sydney   fwd +0-7-0 0:0:0 days=212  inv=0
Australia/Sydney   Australia/Sydney   rev -0-7-0 0:0:0 days=212  inv=1
+05:00             +05:00             fwd +0-7-0 0:0:0 days=212  inv=0
+05:00             +05:00             rev -0-7-0 0:0:0 days=212  inv=1
across 2010-03-13 12:00:00 -> 2010-03-14 12:00:00 : +0-0-1 0:0:0 days=1
across 2010-11-06 12:00:00 -> 2010-11-07 12:00:00 : +0-0-1 0:0:0 days=1
