--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test feof() function
--SKIPIF--
<?php
if (!function_exists('fopen') || !function_exists('feof') || !function_exists('fread') || !function_exists('fclose')) {
    echo 'skip: stream functions not available';
}
?>
--FILE--
<?php
echo "Testing feof() with various file operations\n";

// Create a temporary file with known content
$testFile = tempnam(sys_get_temp_dir(), 'ph7_feof_test');
if (!$testFile) {
    die("Failed to create temp file\n");
}

$content = "Line 1\nLine 2\nLine 3\n";
file_put_contents($testFile, $content);

// Test basic feof functionality
$fp = fopen($testFile, 'r');
if (!$fp) {
    die("Failed to open file\n");
}

echo "Reading file content:\n";
while (!feof($fp)) {
    $line = fgets($fp);
    if ($line !== false) {
        echo "Read: " . trim($line) . "\n";
    }
}

// Should be at EOF now
if (feof($fp)) {
    echo "Correctly detected EOF\n";
} else {
    echo "ERROR: Did not detect EOF\n";
}

fclose($fp);

// Clean up
unlink($testFile);

echo "feof() test completed\n";
?>
--EXPECT--
Testing feof() with various file operations
Reading file content:
Read: Line 1
Read: Line 2
Read: Line 3
Correctly detected EOF
feof() test completed
--CLEAN--
<?php
// Cleanup handled in test
?>