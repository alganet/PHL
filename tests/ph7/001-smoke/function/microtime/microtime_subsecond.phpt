--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: microtime(true) carries sub-second (microsecond) resolution
--FILE--
<?php
// microtime(true) must carry sub-second resolution: across a handful of reads
// at least one has a non-zero fractional part. The pre-fix engine dropped the
// microsecond part, so every read was an exact integer second -> never OK.
// (Sampling instead of a sleep-delta keeps this robust on coarse-grained
// clocks such as Windows' system timer.)
$frac = false;
for ($i = 0; $i < 1000 && !$frac; $i++) {
    $t = microtime(true);
    if ($t - floor($t) > 0) { $frac = true; }
}
echo $frac ? "SUBSEC_OK\n" : "SUBSEC_FAIL\n";

// The string form is PHP's "0.dddddddd sec" (fractional seconds, 8 decimals).
$parts = explode(' ', microtime());
echo (strlen($parts[0]) === 10 && $parts[0][0] === '0' && $parts[0][1] === '.'
      && ctype_digit($parts[1])) ? "FORMAT_OK\n" : "FORMAT_FAIL: {$parts[0]}\n";

// ...and the float form must agree with that string form to ~ms.
$as_float = (float)$parts[1] + (float)$parts[0];
$now = microtime(true);
echo (abs($now - $as_float) < 1) ? "AGREE_OK\n" : "AGREE_FAIL\n";
?>
--EXPECT--
SUBSEC_OK
FORMAT_OK
AGREE_OK
--CLEAN--
<?php
unset($frac, $i, $t, $parts, $as_float, $now);
