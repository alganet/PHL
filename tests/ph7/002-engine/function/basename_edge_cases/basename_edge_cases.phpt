--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
basename edge cases covering additional uncovered lines
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test cases that cover additional uncovered lines in PH7_ExtractDirName via basename
echo "empty_string: '" . basename("") . "'" . PHP_EOL;
echo "no_separators: '" . basename("file") . "'" . PHP_EOL;
echo "root_unix: '" . basename("/") . "'" . PHP_EOL;
echo "multiple_separators: '" . basename("///") . "'" . PHP_EOL;
echo "with_suffix: '" . basename("file.txt", ".txt") . "'" . PHP_EOL;
?>
--EXPECT--
empty_string: ''
no_separators: 'file'
root_unix: '/'
multiple_separators: '/'
with_suffix: 'file'