--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
memory_limit reads php's byte shorthand, and a negative one means no ceiling
--DESCRIPTION--
php's memory_limit is not a plain integer: `128M`, `1G` and `65536` all name a size,
and `-1` -- php's own CLI default -- means unlimited, as does `0`. A parser that
stopped at the first non-digit would read `8M` as 8 BYTES and stop every script on
its next allocation, so the multiplier is proved by BEHAVIOUR and not just by the
round-trip: each ceiling below is set to "what this script has already used, plus
four megabytes, expressed in that unit", and then a two-megabyte string is built
under it. If the suffix were being dropped the ceiling would be a handful of bytes
and each of these would be a fatal.
--SKIPIF--
<?php if (getenv('PHL_MAX_ALLOC')) { echo "skip the per-allocation cap refuses the strings this test sizes against the TOTAL ceiling"; } ?>
--FILE--
<?php
/* Round-trip: ini_get answers the spelling that was written, as php does. */
foreach (['64M', '67108864', '1G', '-1', '0'] as $v) {
    ini_set('memory_limit', $v);
    echo $v, ' => ', ini_get('memory_limit'), "\n";
}

/* The multiplier itself: the same headroom, spelled in each unit. The ceiling is
 * lifted again before anything is REPORTED -- passing the string to strlen() copies
 * it here, so reporting under a tight ceiling would measure that copy and not the
 * multiplier this test is about. */
foreach (['K' => 1024, 'M' => 1024 * 1024] as $unit => $mul) {
    ini_set('memory_limit', '-1');
    $room = memory_get_usage() + (8 * 1024 * 1024);
    ini_set('memory_limit', (intdiv($room, $mul) + 1) . $unit);
    $s = str_repeat('x', 1000000);
    $n = strlen($s);
    ini_set('memory_limit', '-1');
    echo $unit, ' ok len=', $n, "\n";
    unset($s);
}

/* Unlimited really is unlimited. */
ini_set('memory_limit', '-1');
$s = str_repeat('x', 4000000);
echo "unlimited ok len=", strlen($s), "\n";
?>
--EXPECT--
64M => 64M
67108864 => 67108864
1G => 1G
-1 => -1
0 => 0
K ok len=1000000
M ok len=1000000
unlimited ok len=4000000
--CLEAN--
<?php
