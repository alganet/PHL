--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_validate checks JSON validity without materializing a value
--SKIPIF--
<?php if (!function_exists('json_validate')) { die('skip'); } ?>
--FILE--
<?php
// Valid: object, list, and nested structures.
echo json_validate('{"a":1}')      ? "ok\n" : "fail\n";
echo json_validate('[1,2,3]')      ? "ok\n" : "fail\n";
echo json_validate('{"a":{"b":2}}') ? "ok\n" : "fail\n";

// Invalid: garbage and the empty string.
echo json_validate('not json') ? "fail\n" : "ok\n";
echo json_validate('')         ? "fail\n" : "ok\n";

// The last call updates json_last_error().
json_validate('not json');
echo (json_last_error() === JSON_ERROR_SYNTAX) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
ok
ok
ok
ok
ok
--CLEAN--
<?php
