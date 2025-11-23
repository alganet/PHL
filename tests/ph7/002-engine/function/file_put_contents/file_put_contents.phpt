--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test file_put_contents()
--SKIPIF--
<?php

if (PHP_OS === 'WINNT' && function_exists('zend_version')) {
    echo "skip: platform";
}
if (!function_exists('file_put_contents') || !function_exists('flock') || !function_exists('fopen')) {
    echo 'skip: file functions not available';
}
?>
--FILE--
<?php
echo "Testing file_put_contents with empty data\n";

// Test 1: Empty data should succeed and create file
$testFile = tempnam(sys_get_temp_dir(), 'ph7_empty_put_test');
$result = file_put_contents($testFile, '');
if ($result === 0) {
    echo "Empty file_put_contents returned 0 (success)\n";
} else {
    echo "ERROR: Empty file_put_contents returned $result\n";
}

// Verify file exists and is empty
if (file_exists($testFile) && filesize($testFile) === 0) {
    echo "File created and is empty\n";
} else {
    echo "ERROR: File not created or not empty\n";
}

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
unlink($testFile);
unlink($lockFile);

echo "file_put_contents test completed\n";
?>
--EXPECT--
Testing file_put_contents with empty data
Empty file_put_contents returned 0 (success)
File created and is empty
Writing to locked file succeeded (advisory lock)
file_put_contents test completed
--CLEAN--
<?php
// Cleanup handled in test
?>