--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode keeps lists and empty arrays as JSON arrays
--SKIPIF--
<?php if (!function_exists('json_encode')) { die('skip'); } ?>
--FILE--
<?php
echo json_encode([0 => "a", 1 => "b"]), "\n"; // consecutive 0-based keys -> array
echo json_encode(["a", "b", "c"]), "\n";       // natural list -> array
echo json_encode([]), "\n";                     // empty stays an array
echo json_encode([[1, 2], [3, 4]]), "\n";       // list of lists
?>
--EXPECT--
["a","b"]
["a","b","c"]
[]
[[1,2],[3,4]]
