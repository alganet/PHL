--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with no arguments
$result = file();
echo "no_args: " . count($result) . "\n";

// Test with invalid file path
$result = file("/nonexistent/path/file.txt");
if ($result === false) {
    echo "invalid_path: false\n";
} else {
    echo "invalid_path: " . count($result) . "\n";
}

// Test with array argument
$result = file(array("test"));
if ($result === false) {
    echo "array_arg: false\n";
} else {
    echo "array_arg: " . count($result) . "\n";
}
?>
--EXPECTF--
%s Warning:  file(): Expecting a file path
no_args: %d
%s Error:  file(): IO error while opening '/nonexistent/path/file.txt'
invalid_path: false
%s Warning:  file(): Expecting a file path
array_arg: false
--CLEAN--
<?php
unset($result);
