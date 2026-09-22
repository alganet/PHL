--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
convert_uudecode() decodes uuencoded data (not base64)
--DESCRIPTION--
The builtin used to be registered as an alias of base64_decode(), so it answered
garbage for real uuencoded input. It reads the length byte of each line, keeps only
as many bytes as the length bytes declare, stops at the first short line (trailing
text is ignored) and accepts input with or without the "`\n" end marker.
--FILE--
<?php
var_dump(convert_uudecode("#86)C\n`\n"));
var_dump(convert_uudecode("!80``\n`\n"));
var_dump(convert_uudecode("#86)C"));
var_dump(convert_uudecode("#86)Ctrailing junk"));
var_dump(convert_uudecode("`\n"));
var_dump(convert_uudecode(" "));
var_dump(convert_uudecode(convert_uuencode("round trip")));
?>
--EXPECT--
string(3) "abc"
string(1) "a"
string(3) "abc"
string(3) "abc"
string(0) ""
string(0) ""
string(10) "round trip"
--CLEAN--
<?php
