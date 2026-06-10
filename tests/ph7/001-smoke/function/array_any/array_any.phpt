--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_any: true if any element matches; false for empty
--SKIPIF--
<?php if (!function_exists('array_any')) echo 'skip array_any unavailable'; ?>
--FILE--
<?php
// No top-level helper functions: the in-process smoke runner includes every
// test into one process, so a named function would collide across files.
echo array_any([1, 2, 3], fn($v) => $v > 2) ? "true" : "false", "\n";   // true
echo array_any([1, 2, 3], fn($v) => $v > 9) ? "true" : "false", "\n";   // false
echo array_any([], fn($v) => true) ? "true" : "false", "\n";            // false (empty)
echo array_any(['a' => 1, 'b' => 2], fn($v, $k) => $k === 'b') ? "true" : "false", "\n";
?>
--EXPECT--
true
false
false
true
--CLEAN--
<?php
?>
