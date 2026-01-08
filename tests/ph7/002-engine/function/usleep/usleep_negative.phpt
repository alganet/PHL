--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
usleep() with negative value should return immediately
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test usleep with negative value - should return immediately without error
$start = microtime(true);
$result = usleep(-1000);  // Negative value
$end = microtime(true);
$elapsed = ($end - $start) * 1000000; // Convert to microseconds

// Should return NULL (no return value) and not sleep
if ($result === null && $elapsed < 500) {  // Less than 500 microseconds
    echo "PASS: usleep(-1000) returned immediately\n";
} else {
    echo "FAIL: usleep(-1000) behavior unexpected\n";
}
?>
--EXPECT--
PASS: usleep(-1000) returned immediately
