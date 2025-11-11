--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test file_put_contents()
Compatibility test with Zend PHP on Windows
--SKIPIF--
<?php

if (PHP_OS === 'Linux' || !function_exists('zend_version')) {
    echo "skip: platform";
}
if (!function_exists('file_put_contents') || !function_exists('flock') || !function_exists('fopen')) {
    echo 'skip: file functions not available';
}
?>
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
?>