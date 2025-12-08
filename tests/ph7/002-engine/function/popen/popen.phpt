--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test popen() and pclose() functions
--SKIPIF--
<?php
if (!function_exists('popen') || !function_exists('pclose')) {
    echo 'skip: popen/pclose functions not available';
}
?>
--FILE--
<?php
echo "Testing popen() basic functionality\n";

// Test 1: Read from a simple echo command
// Use 'echo' which works on both Windows and Unix
if (PHP_OS == 'WINNT') {
    $cmd = 'echo Hello from popen';
} else {
    $cmd = 'echo "Hello from popen"';
}

$fp = popen($cmd, 'r');
if ($fp === false) {
    echo "ERROR: popen() returned false\n";
} else {
    echo "popen() returned a resource: " . (is_resource($fp) ? 'yes' : 'no') . "\n";

    // Read the output using fgets
    $output = fgets($fp);
    $output = trim($output);
    echo "Output: " . $output . "\n";

    // Close the pipe
    $status = pclose($fp);
    echo "pclose() returned: " . $status . "\n";
}

// Test 2: Test fread with popen
echo "Testing fread() with popen()\n";
if (PHP_OS == 'WINNT') {
    $cmd = 'echo TestData';
} else {
    $cmd = 'echo "TestData"';
}

$fp = popen($cmd, 'r');
if ($fp !== false) {
    $data = fread($fp, 100);
    $data = trim($data);
    echo "fread() result: " . $data . "\n";
    pclose($fp);
}

// Test 3: Test reading multiple lines
echo "Testing multiple line output\n";
if (PHP_OS == 'WINNT') {
    $cmd = 'echo Line1 && echo Line2';
} else {
    $cmd = 'echo "Line1" && echo "Line2"';
}

$fp = popen($cmd, 'r');
if ($fp !== false) {
    $line1 = trim(fgets($fp));
    $line2 = trim(fgets($fp));
    echo "First line: " . $line1 . "\n";
    echo "Second line: " . $line2 . "\n";
    pclose($fp);
}

echo "popen() test completed\n";
?>
--EXPECT--
Testing popen() basic functionality
popen() returned a resource: yes
Output: Hello from popen
pclose() returned: 0
Testing fread() with popen()
fread() result: TestData
Testing multiple line output
First line: Line1
Second line: Line2
popen() test completed
