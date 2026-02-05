--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Single-quoted strings with verbatim unknown escape sequences
--FILE--
<?php
// Test single-quoted strings with unknown escape sequences
// Covers PH7_CompileSimpleString verbatim copy path (lines ~436-446)

$test1 = 'hello\xworld';
$test2 = 'test\yvalue';
$test3 = 'foo\zbar';

echo $test1 . "\n";
echo $test2 . "\n";
echo $test3 . "\n";

echo "Done\n";
?>
--EXPECT--
hello\xworld
test\yvalue
foo\zbar
Done
--CLEAN--
<?php
unset($test1, $test2, $test3);
