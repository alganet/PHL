--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test fgets() function
--SKIPIF--
<?php
if (!function_exists('fopen') || !function_exists('fgets') || !function_exists('fclose')) {
    echo 'skip: stream functions not available';
}
?>
--FILE--
<?php
echo "Testing fgets() basic functionality\n";

// Create a temporary file with known content
$testFile = tempnam(sys_get_temp_dir(), 'ph7_fgets_test');
if (!$testFile) {
    die("Failed to create temp file\n");
}

$content = "Line 1\nLine 2\nLine 3\n";
file_put_contents($testFile, $content);

// Test fgets() without length limit
$fp = fopen($testFile, 'r');
if (!$fp) {
    die("Failed to open file\n");
}

echo "Reading with fgets():\n";
$lines = array();
while (!feof($fp)) {
    $line = fgets($fp);
    if ($line !== false) {
        $lines[] = trim($line);
        echo "Read: " . trim($line) . "\n";
    }
}
fclose($fp);

// Verify we read the correct lines
if (count($lines) == 3 && $lines[0] == 'Line 1' && $lines[1] == 'Line 2' && $lines[2] == 'Line 3') {
    echo "Correctly read all lines\n";
} else {
    echo "ERROR: Did not read lines correctly\n";
}

// Test with empty file
$emptyFile = tempnam(sys_get_temp_dir(), 'ph7_empty_fgets_test');
file_put_contents($emptyFile, '');

$fp = fopen($emptyFile, 'r');
$line = fgets($fp);
if ($line === false) {
    echo "Correctly returned false for empty file\n";
} else {
    echo "ERROR: Did not return false for empty file\n";
}
fclose($fp);

// Clean up
unlink($testFile);
unlink($emptyFile);

echo "fgets() test completed\n";
?>
--EXPECT--
Testing fgets() basic functionality
Reading with fgets():
Read: Line 1
Read: Line 2
Read: Line 3
Correctly read all lines
Correctly returned false for empty file
fgets() test completed
--CLEAN--
<?php
// Cleanup handled in test
?>