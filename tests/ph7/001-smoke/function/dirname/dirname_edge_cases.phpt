--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
dirname edge cases covering uncovered lines
--SKIPIF--
<?php if (PHP_OS == 'WINNT' || function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test cases that cover uncovered lines in PH7_ExtractDirName
echo "empty_string: '" . dirname("") . "'" . PHP_EOL;
echo "no_separators: '" . dirname("file") . "'" . PHP_EOL;
echo "root_unix: '" . dirname("/") . "'" . PHP_EOL;
echo "multiple_separators: '" . dirname("///") . "'" . PHP_EOL;
?>
--EXPECT--
empty_string: ''
no_separators: '.'
root_unix: '/'
multiple_separators: '//'
--CLEAN--
<?php

