--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file with invalid arguments (PHL-native diagnostics)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with invalid file path
$result = file("/nonexistent/path/file.txt");
echo "invalid_path: " . (is_array($result) ? count($result) : "false") . "\n";

// Test with array argument
$result = file(array("test"));
echo "array_arg: " . (is_array($result) ? count($result) : "false") . "\n";
?>
--EXPECTF--
Error: file(): IO error while opening '/nonexistent/path/file.txt' in %s on line %d
invalid_path: false
Warning: file(): Expecting a file path in %s on line %d
array_arg: false
--CLEAN--
<?php
unset($result);
