--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
copy with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with no arguments
$result = copy();
echo "no_args: $result\n";

// Test with one argument
$result = copy("source.txt");
echo "one_arg: $result\n";

// Test with invalid source
$result = copy("/nonexistent/source.txt", "/tmp/dest.txt");
echo "invalid_source: $result\n";

// Test with invalid destination type
$result = copy("source.txt", array("dest"));
echo "invalid_dest: $result\n";
?>
--EXPECTF--
Warning: copy(): Expecting a source and a destination path in %s on line %d
no_args: 
Warning: copy(): Expecting a source and a destination path in %s on line %d
one_arg: 
Error: copy(): IO error while opening source: '/nonexistent/source.txt' in %s on line %d
invalid_source: 
Warning: copy(): Expecting a source and a destination path in %s on line %d
invalid_dest:
--CLEAN--
<?php
unset($result);
