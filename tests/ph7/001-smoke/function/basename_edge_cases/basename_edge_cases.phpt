--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
basename edge cases covering additional uncovered lines
--FILE--
<?php
// Test cases that cover additional uncovered lines in PH7_ExtractDirName via basename
echo "empty_string: '" . basename("") . "'" . PHP_EOL;
echo "no_separators: '" . basename("file") . "'" . PHP_EOL;
echo "root_unix: '" . basename("/") . "'" . PHP_EOL;
echo "multiple_separators: '" . basename("///") . "'" . PHP_EOL;
echo "with_suffix: '" . basename("file.txt", ".txt") . "'" . PHP_EOL;
?>
--EXPECTF--
%Aempty_string: ''%Ano_separators: 'file'%Aroot_unix: ''%Amultiple_separators: ''%Awith_suffix: 'file'%A
--CLEAN--
<?php

