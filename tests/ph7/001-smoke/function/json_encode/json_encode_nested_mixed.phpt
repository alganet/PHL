--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_encode handles object/list nesting and JSON_FORCE_OBJECT
--SKIPIF--
<?php if (!function_exists('json_encode') || !function_exists('json_decode')) { die('skip'); } ?>
--FILE--
<?php
// object level whose values are a list then a nested object: exercises the
// save/restore of the per-level object flag across sibling entries.
echo json_encode(["a" => [1, 2], "b" => ["c" => 3]]), "\n";

// JSON_FORCE_OBJECT still forces a list to be an object
echo json_encode(["a", "b"], JSON_FORCE_OBJECT), "\n";

// round-trip: an associative array decodes back associatively
$src = ["id" => 7, "tags" => ["x", "y"]];
$back = json_decode(json_encode($src), true);
echo ($back["id"] === 7 && $back["tags"][1] === "y" ? "ok" : "fail"), "\n";
?>
--EXPECT--
{"a":[1,2],"b":{"c":3}}
{"0":"a","1":"b"}
ok
