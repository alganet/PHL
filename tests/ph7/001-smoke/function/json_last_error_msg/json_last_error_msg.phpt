--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_last_error_msg reports the message for the last JSON error
--SKIPIF--
<?php if (!function_exists('json_last_error_msg')) { die('skip'); } ?>
--FILE--
<?php
// A successful decode leaves no error.
json_decode('{"a":1}', true);
echo json_last_error_msg(), "\n";

// A malformed input yields the syntax-error message.
json_decode('invalid json');
echo json_last_error_msg(), "\n";
?>
--EXPECT--
No error
Syntax error
--CLEAN--
<?php
