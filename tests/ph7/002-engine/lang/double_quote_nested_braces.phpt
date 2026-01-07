--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Lexer handles nested braces in double-quoted strings with newlines
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test nested braces inside double-quoted strings (lex.c lines 285-298, 336)
// This covers the brace nesting counter and newline handling in string interpolation

$arr = array('key' => 'value', 'nested' => array('inner' => 'deep'));
$key = 'key';

// Simple brace interpolation
echo "Simple: {$arr['key']}\n";

// Nested array access with braces
$nested_key = 'inner';
echo "Nested: {$arr['nested']['inner']}\n";

// Multi-line content inside braces in double quoted string
$multiline = "Start {$arr['key']} 
middle line
end {$arr['nested']['inner']} finish";
echo "Has newline: " . (strpos($multiline, "\n") !== false ? "OK" : "FAIL") . "\n";
echo "Has value: " . (strpos($multiline, "value") !== false ? "OK" : "FAIL") . "\n";
echo "Has deep: " . (strpos($multiline, "deep") !== false ? "OK" : "FAIL") . "\n";

echo "Done\n";
?>
--EXPECT--
Simple: value
Nested: deep
Has newline: OK
Has value: OK
Has deep: OK
Done