--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_FLOAT: reject out-of-range magnitudes (overflow + underflow-to-zero)
--FILE--
<?php
/* PHP rejects a float whose magnitude overflows the double range (-> +/-INF) or
 * totally underflows to zero, but accepts subnormals (nonzero) and a genuine 0. */

/* Rejections: must all be === false. */
foreach ([
    "1e400", "-1e400", "1.8e308", "2e308", /* overflow */
    "1e-400", "1e-330",                    /* underflow to zero */
] as $x) {
    echo (filter_var($x, FILTER_VALIDATE_FLOAT) === false) ? "reject\n" : "WRONG\n";
}
/* Overflow through the thousand-separator path too. */
echo (filter_var("1,000e400", FILTER_VALIDATE_FLOAT, FILTER_FLAG_ALLOW_THOUSAND) === false) ? "reject\n" : "WRONG\n";

/* Acceptances: a float in the expected ballpark. Ranges (not exact strings) keep
 * the test cross-engine despite the extreme-magnitude float->string rendering gap. */
$acc = [
    ["1e308",   1e307,   1e309],
    ["1.7e308", 1e308,   1.8e308],
    ["1e-320",  0.0,     1e-319],   /* subnormal, nonzero */
    ["5e-324",  0.0,     1e-323],   /* smallest subnormal */
    ["1.5e300", 1e299,   1e301],
    ["0",      -1.0,     1.0],      /* genuine zero, not an underflow */
    ["1.5",     1.0,     2.0],
];
foreach ($acc as [$x, $lo, $hi]) {
    $v = filter_var($x, FILTER_VALIDATE_FLOAT);
    echo (is_float($v) && $v >= $lo && $v <= $hi) ? "ok\n" : "WRONG\n";
}
?>
--EXPECT--
reject
reject
reject
reject
reject
reject
reject
ok
ok
ok
ok
ok
ok
ok
