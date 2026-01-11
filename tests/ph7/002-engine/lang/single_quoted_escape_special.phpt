--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Single-quoted strings with special escape sequences
--FILE--
<?php
// Test single-quoted strings with \ and ' escapes
// Covers PH7_CompileSimpleString escape handling (lines ~436, 445)

$test1 = 'It\'s a test';
$test2 = 'Path\\to\\file';
$test3 = 'Quote\' and backslash\\';

echo $test1 . "\n";
echo $test2 . "\n";
echo $test3 . "\n";

echo "Done\n";
?>
--EXPECT--
It's a test
Path\to\file
Quote' and backslash\
Done
--CLEAN--
<?php
unset($test1, $test2, $test3);
?>