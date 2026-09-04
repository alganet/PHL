--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php
// Windows-only semantics: this asserts a MANDATORY file lock (write fails with
// EACCES). POSIX locks are advisory, so the write succeeds on every unix — the
// test is about the platform, not the engine. My guard-lift sweep wrongly
// unskipped it because both engines AGREE here; agreement is not the same as
// passing the expectation.
if (PHP_OS !== 'WINNT' || !function_exists('zend_version')) { echo 'skip Windows-only: POSIX locks are advisory'; }
?>
--TEST--
Test file_put_contents()
Compatibility test with Zend PHP on Windows

--FILE--
<?php

$restore_error_handler = set_error_handler(function ($code, $msg) {
    echo "CAUGHT: $msg" . PHP_EOL;
    return true;
});

// Test 2: Try writing to a locked file (advisory lock, may not fail on all systems)
$lockFile = tempnam(sys_get_temp_dir(), 'ph7_lock_test');
$fp = fopen($lockFile, 'w');
if ($fp && flock($fp, LOCK_EX)) {
    // File is locked, try to write to it
    $result = file_put_contents($lockFile, 'test');
    if ($result === false) {
        echo "Writing to locked file failed as expected\n";
    } else {
        echo "Writing to locked file succeeded (advisory lock)\n";
    }
    flock($fp, LOCK_UN);
    fclose($fp);
} else {
    echo "Could not lock file for testing\n";
}

// Clean up
unlink($lockFile);
set_error_handler($restore_error_handler);

?>
--EXPECT--
CAUGHT: file_put_contents(): Write of 4 bytes failed with errno=13 Permission denied
Writing to locked file failed as expected
--CLEAN--
<?php
// Cleanup handled in test
unset($restore_error_handler, $lockFile, $fp, $result);
