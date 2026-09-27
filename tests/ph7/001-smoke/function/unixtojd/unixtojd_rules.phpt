--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unixtojd() converts the LOCAL date a timestamp falls on
--FILE--
<?php
/* unixtojd() is NOT the plain inverse of jdtounix(): php breaks the timestamp
 * down with the C library's localtime() -- the PROCESS timezone, not
 * date.timezone -- and converts the calendar date it finds. So every assertion
 * here is relational rather than a pinned counter: the answers move with the
 * box's zone, and both engines move with it together. */
function unixtojd_rules(): void {
    echo "## the counter changes exactly once in any twenty-four hours\n";
    $ujd_t0 = jdtounix(2451545);
    $ujd_changes = 0;
    for ($ujd_s = 60; $ujd_s <= 86400; $ujd_s += 60) {
        if (unixtojd($ujd_t0 + $ujd_s) !== unixtojd($ujd_t0 + $ujd_s - 60)) { $ujd_changes++; }
    }
    printf("counter changes in 24h: %d\n", $ujd_changes);

    echo "## and it advances by exactly one a day\n";
    $bad = 0;
    $ujd_base = jdtounix(2451545) + 43200;
    for ($n = 0; $n < 400; $n++) {
        if (unixtojd($ujd_base + $n * 86400) !== unixtojd($ujd_base) + $n) { $bad++; }
    }
    printf("consecutive-day disagreements: %d\n", $bad);

    echo "## noon UTC is the same date in every real zone, so it round-trips\n";
    $bad = 0;
    for ($jd = 2440588; $jd < 2470000; $jd += 17) {
        if (unixtojd(jdtounix($jd) + 43200) !== $jd) { $bad++; }
    }
    printf("round-trip mismatches: %d\n", $bad);

    echo "## it agrees with gregoriantojd() about the date it found\n";
    $bad = 0;
    for ($ts = 43200; $ts < 2000000000; $ts += 999983) {
        $jd = unixtojd($ts);
        [$m, $d, $y] = array_map('intval', explode('/', jdtogregorian($jd)));
        if (gregoriantojd($m, $d, $y) !== $jd) { $bad++; }
    }
    printf("disagreements: %d\n", $bad);

    echo "## a negative timestamp is refused, not counted backwards\n";
    foreach ([-1, -86400, PHP_INT_MIN] as $ts) {
        try { printf("%-21d %d\n", $ts, unixtojd($ts)); }
        catch (ValueError $e) { printf("%-21d %s\n", $ts, $e->getMessage()); }
    }

    echo "## no argument -- and an explicit null -- are the current time\n";
    $now = unixtojd();
    var_dump($now === unixtojd(time()), $now === unixtojd(null),
        $now > 2440588, $now < 3000000);
}
unixtojd_rules();
--EXPECT--
## the counter changes exactly once in any twenty-four hours
counter changes in 24h: 1
## and it advances by exactly one a day
consecutive-day disagreements: 0
## noon UTC is the same date in every real zone, so it round-trips
round-trip mismatches: 0
## it agrees with gregoriantojd() about the date it found
disagreements: 0
## a negative timestamp is refused, not counted backwards
-1                    unixtojd(): Argument #1 ($timestamp) must be greater than or equal to 0
-86400                unixtojd(): Argument #1 ($timestamp) must be greater than or equal to 0
-9223372036854775808  unixtojd(): Argument #1 ($timestamp) must be greater than or equal to 0
## no argument -- and an explicit null -- are the current time
bool(true)
bool(true)
bool(true)
bool(true)
