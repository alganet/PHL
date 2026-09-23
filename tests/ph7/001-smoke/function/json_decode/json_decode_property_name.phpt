--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object-mode json_decode() refuses a property name with a LEADING NUL (php error 9)
--FILE--
<?php
$lead = '{"\u0000a":1}';
$mid  = '{"a\u0000b":1}';
// Object mode: leading NUL collides with php's mangled-name prefix.
var_dump(json_decode($lead));
echo json_last_error(), " ", json_last_error_msg(), "\n";
// Array modes take any key; a NUL further in is legal even for objects.
var_dump(json_decode($lead, true) !== null, json_last_error());
var_dump(json_decode($lead, null, 512, JSON_OBJECT_AS_ARRAY) !== null, json_last_error());
var_dump(json_decode($mid) !== null, json_last_error());
// json_validate() decodes in array mode, so the document is VALID.
var_dump(json_validate($lead), json_last_error());
?>
--EXPECT--
NULL
9 The decoded property name is invalid
bool(true)
int(0)
bool(true)
int(0)
bool(true)
int(0)
bool(true)
int(0)
--CLEAN--
<?php
