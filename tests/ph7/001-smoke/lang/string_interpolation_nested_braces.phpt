--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation with nested curly braces
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test nested curly braces in string interpolation
// Covers GenStateCompileString brace nesting logic

$arr = array('key1' => 'value1', 'key2' => 'value2');
$key = 'key1';

echo "Nested: {$arr{$key}}\n";

$deep = array('inner' => array('deep' => 'found'));
$outer = 'inner';
$inner = 'deep';

echo "Deep nested: {$deep{$outer}{$inner}}\n";

echo "Done\n";
?>
--EXPECT--
Nested: value1
Deep nested: found
Done
--CLEAN--
<?php
unset($arr, $key, $deep, $outer, $inner);
