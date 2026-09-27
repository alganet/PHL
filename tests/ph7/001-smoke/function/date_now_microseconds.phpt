--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The current moment carries its microseconds into a date object
--FILE--
<?php
/* php's date classes take their base moment from the clock microtime() reads,
 * sub-second part included, so `new DateTime()` is born on the microsecond it
 * was built on. PHL read `time(0)` and left the base microseconds at zero, so
 * every such object sat on a whole second and `$a->diff($b)->f` between two
 * moments a program measured was always 0.0.
 *
 * A single draw could legitimately land on .000000, so the probes below take
 * many: a run where all of them are zero is a one-in-10^120 event, not a flake. */
$tz = date_default_timezone_get();
date_default_timezone_set('UTC');

function anyNonZero(callable $make): bool {
    for ($i = 0; $i < 20; $i++) {
        if ($make()->format('u') !== '000000') {
            return true;
        }
    }
    return false;
}
/* the four doors onto "now" */
var_dump(anyNonZero(fn() => new DateTime()));
var_dump(anyNonZero(fn() => new DateTime('')));
var_dump(anyNonZero(fn() => new DateTimeImmutable('now')));
var_dump(anyNonZero(fn() => date_create()));
/* a relative string names no time of day, so the base clock survives it */
var_dump(anyNonZero(fn() => new DateTime('+1 day')));
/* ...and a string that DOES name one zeroes the microseconds with the rest of
 * the clock, whether it spells the time or asks for midnight */
var_dump((new DateTime('2020-01-01'))->format('u'));
var_dump((new DateTime('2020-01-01 12:00'))->format('u'));
var_dump((new DateTime('12:00'))->format('u'));
var_dump((new DateTime('tomorrow'))->format('u'));
var_dump((new DateTime('@1600000000'))->format('u'));
/* php's createFromFormat fills the fields its format never named from the
 * current clock, but its microseconds start at zero */
var_dump(DateTime::createFromFormat('Y-m-d', '2020-01-01')->format('u'));
/* a BARE four-digit run php reads as a year keeps the current time of day and
 * still answers nothing under the second: the run reached that reading through
 * php's have_time bookkeeping, which zeroes the sub-second field on the way */
var_dump((new DateTime('7609'))->format('u'));
var_dump((new DateTime('9999 +1 day'))->format('u'));
var_dump((new DateTime('7609'))->format('Y'));
/* the whole point: two moments a program measures differ by a real fraction */
$a = new DateTime();
usleep(2000);
$b = new DateTime();
var_dump($b->diff($a)->f > 0.0);
var_dump($b > $a);

date_default_timezone_set($tz);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(6) "000000"
string(6) "000000"
string(6) "000000"
string(6) "000000"
string(6) "000000"
string(6) "000000"
string(6) "000000"
string(6) "000000"
string(4) "7609"
bool(true)
bool(true)
