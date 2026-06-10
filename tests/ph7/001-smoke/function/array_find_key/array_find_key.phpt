--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_find_key: first matching key (int and string keys), null when none
--SKIPIF--
<?php if (!function_exists('array_find_key')) echo 'skip array_find_key unavailable'; ?>
--FILE--
<?php
// No top-level helper functions: the in-process smoke runner includes every
// test into one process, so a named function would collide across files.
echo ($r = array_find_key(['a' => 1, 'b' => 2, 'c' => 3], fn($v, $k) => $v > 1)) === null ? 'null' : (string) $r, "\n"; // b
echo ($r = array_find_key([10, 20, 30], fn($v, $k) => $v === 30)) === null ? 'null' : (string) $r, "\n";               // 2 (int key)
echo ($r = array_find_key([1, 2, 3], fn($v) => $v > 9)) === null ? 'null' : (string) $r, "\n";                         // null
?>
--EXPECT--
b
2
null
--CLEAN--
<?php
?>
