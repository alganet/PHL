--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode emits a JSON object for non-list arrays
--SKIPIF--
<?php if (!function_exists('json_encode')) { die('skip'); } ?>
--FILE--
<?php
echo json_encode(["x" => 1]), "\n";          // string keys -> object
echo json_encode([1 => "a", 2 => "b"]), "\n"; // int keys, non-zero start -> object
echo json_encode([0 => "a", 2 => "b"]), "\n"; // gap in int keys -> object
echo json_encode(["a" => 1, 0 => "x", 1 => "y"]), "\n"; // mixed -> object
?>
--EXPECT--
{"x":1}
{"1":"a","2":"b"}
{"0":"a","2":"b"}
{"a":1,"0":"x","1":"y"}
