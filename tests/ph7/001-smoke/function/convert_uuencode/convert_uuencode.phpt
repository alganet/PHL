--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
convert_uuencode() produces uuencoded data (not base64)
--DESCRIPTION--
The builtin used to be registered as an alias of base64_encode(), so every answer
was silently a base64 string. Values pinned against php 8.5: a line is its encoded
length byte, four characters per three input bytes, a newline, and php's "`\n" end
marker; a group short of three bytes is filled with backticks.
--FILE--
<?php
var_dump(convert_uuencode('abc'));
var_dump(convert_uuencode('a'));
var_dump(convert_uuencode('ab'));
var_dump(convert_uuencode(''));
var_dump(convert_uuencode("\x00\x00\x00"));
var_dump(bin2hex(convert_uuencode("\xff\xfe\xfd")));
?>
--EXPECT--
string(8) "#86)C
`
"
string(8) "!80``
`
"
string(8) ""86(`
`
"
string(2) "`
"
string(8) "#````
`
"
string(16) "235f5f5b5d0a600a"
--CLEAN--
<?php
