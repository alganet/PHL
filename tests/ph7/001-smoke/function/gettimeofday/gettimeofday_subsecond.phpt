--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
gettimeofday(true) carries sub-second resolution; usec is in range
--SKIPIF--
<?php
if (!function_exists('gettimeofday')) { echo 'skip: gettimeofday not available'; }
?>
--FILE--
<?php
// gettimeofday(true) must carry sub-second resolution: across a handful of
// reads at least one has a non-zero fractional part (was always 0 before the
// fix, which dropped tm_usec). Sampling keeps this robust on coarse clocks.
$frac = false;
for ($i = 0; $i < 1000 && !$frac; $i++) {
    $t = gettimeofday(true);
    if ($t - floor($t) > 0) { $frac = true; }
}
echo $frac ? "SUBSEC_OK\n" : "SUBSEC_FAIL\n";

// The array form's usec field must be a valid microsecond count.
$arr = gettimeofday();
$u = $arr['usec'];
echo ($u >= 0 && $u < 1000000) ? "USEC_RANGE_OK\n" : "USEC_RANGE_FAIL\n";
?>
--EXPECT--
SUBSEC_OK
USEC_RANGE_OK
--CLEAN--
<?php
unset($frac, $i, $t, $arr, $u);
