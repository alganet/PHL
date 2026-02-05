--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test pclose() function exit status
--FILE--
<?php
echo "Testing pclose() exit status\n";

// Test 1: Command that exits with status 0
if (PHP_OS == 'WINNT') {
    $cmd = 'exit /b 0';
} else {
    $cmd = 'true';
}
$fp = popen($cmd, 'r');
if ($fp !== false) {
    $status = pclose($fp);
    echo "Exit 0 command status: " . $status . "\n";
}

// Test 2: Command that exits with non-zero status
if (PHP_OS == 'WINNT') {
    $cmd = 'exit /b 1';
} else {
    $cmd = 'false';
}
$fp = popen($cmd, 'r');
if ($fp !== false) {
    $status = pclose($fp);
    echo "Exit 1 command status: " . $status . "\n";
}

// Test 3: Command that exits with status 42
if (PHP_OS == 'WINNT') {
    $cmd = 'exit /b 42';
} else {
    $cmd = 'sh -c "exit 42"';
}
$fp = popen($cmd, 'r');
if ($fp !== false) {
    $status = pclose($fp);
    echo "Exit 42 command status: " . $status . "\n";
}

echo "pclose() test completed\n";
?>
--EXPECT--
Testing pclose() exit status
Exit 0 command status: 0
Exit 1 command status: 1
Exit 42 command status: 42
pclose() test completed
--CLEAN--
<?php
unset($cmd, $fp, $status);
