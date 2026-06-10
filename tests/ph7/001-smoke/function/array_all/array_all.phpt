--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_all: true only if every element matches; true for empty
--SKIPIF--
<?php if (!function_exists('array_all')) echo 'skip array_all unavailable'; ?>
--FILE--
<?php
// No top-level helper functions: the in-process smoke runner includes every
// test into one process, so a named function would collide across files.
echo array_all([1, 2, 3], fn($v) => $v > 0) ? "true" : "false", "\n";   // true
echo array_all([1, 2, 3], fn($v) => $v > 1) ? "true" : "false", "\n";   // false
echo array_all([], fn($v) => false) ? "true" : "false", "\n";           // true (empty)
echo array_all(['a' => 1, 'b' => 2], fn($v, $k) => is_string($k)) ? "true" : "false", "\n"; // true (key arg)
?>
--EXPECT--
true
false
true
true
--CLEAN--
<?php
?>
