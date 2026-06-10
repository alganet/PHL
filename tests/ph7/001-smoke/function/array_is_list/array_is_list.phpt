--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_is_list: lists vs non-lists
--SKIPIF--
<?php if (!function_exists('array_is_list')) echo 'skip array_is_list unavailable'; ?>
--FILE--
<?php
echo json_encode([
    array_is_list([]),                 // true
    array_is_list([1, 2, 3]),          // true
    array_is_list(['a', 'b']),         // true (keys 0,1)
    array_is_list([1 => 'a']),         // false (starts at 1)
    array_is_list(['a' => 1]),         // false (string key)
    array_is_list([0 => 'a', 2 => 'b']), // false (gap)
]), "\n";
?>
--EXPECT--
[true,true,true,false,false,false]
--CLEAN--
<?php
?>
