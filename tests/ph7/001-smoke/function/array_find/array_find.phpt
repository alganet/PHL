--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_find: first matching value, callback receives ($value, $key)
--SKIPIF--
<?php if (!function_exists('array_find')) echo 'skip array_find unavailable'; ?>
--FILE--
<?php
// No top-level helper functions: the in-process smoke runner includes every
// test into one process, so a named function would collide across files.
echo ($r = array_find(['a' => 1, 'b' => 2, 'c' => 3], fn($v, $k) => $v > 1)) === null ? 'null' : (string) $r, "\n";  // 2
echo ($r = array_find(['a' => 1, 'b' => 2], fn($v, $k) => $k === 'b')) === null ? 'null' : (string) $r, "\n";        // 2
echo ($r = array_find([1, 2, 3], fn($v) => $v > 9)) === null ? 'null' : (string) $r, "\n";                           // null
?>
--EXPECT--
2
2
null
--CLEAN--
<?php
?>
