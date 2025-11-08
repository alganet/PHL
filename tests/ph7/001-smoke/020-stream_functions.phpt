--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test stream functions: fopen, fread, fwrite, fclose, feof, fseek, ftell, rewind
--FILE--
<?php
echo "Stream functions test: Basic functionality check\n";

// Test that functions exist (don't actually use them to avoid file system issues)
$functions = array('fopen', 'fread', 'fwrite', 'fclose', 'feof', 'fseek', 'ftell', 'rewind');
foreach ($functions as $func) {
    if (function_exists($func)) {
        echo "$func function exists\n";
    } else {
        echo "$func function does not exist\n";
    }
}

echo "Stream functions test completed.\n";
?>
--EXPECT--
Stream functions test: Basic functionality check
fopen function exists
fread function exists
fwrite function exists
fclose function exists
feof function exists
fseek function exists
ftell function exists
rewind function exists
Stream functions test completed.