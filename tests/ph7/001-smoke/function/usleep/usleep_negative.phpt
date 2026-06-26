--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
usleep() with negative value should return immediately
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// usleep() ignores a negative delay and returns immediately (PHL extension;
// PHP 8 raises a ValueError, hence the --SKIPIF-- above). The C implementation
// guards `nSleep < 0` and never calls the sleep routine, so the only real
// regression — dropping that guard — casts -1000 to a huge unsigned delay and
// HANGS, rather than adding a few hundred microseconds. The old sub-millisecond
// timing bound therefore caught no real bug and only flaked on CI scheduling
// jitter; use a generous bound that tolerates that jitter while still flagging a
// gross sleep.
$start = microtime(true);
$result = usleep(-1000);  // Negative value
$elapsed = (microtime(true) - $start) * 1000000; // microseconds

// Must return NULL (no return value) and not sleep. 100 ms is orders of
// magnitude above any plausible scheduling jitter for a no-op, yet far below
// the multi-second sleep a missing negative-guard would produce.
if ($result === null && $elapsed < 100000) {
    echo "PASS: usleep(-1000) returned immediately\n";
} else {
    echo "FAIL: usleep(-1000) behavior unexpected (result=",
        var_export($result, true), ", elapsed=", (int)$elapsed, "us)\n";
}
?>
--EXPECT--
PASS: usleep(-1000) returned immediately
--CLEAN--
<?php
unset($start, $result, $elapsed);
