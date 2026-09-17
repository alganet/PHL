--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String comparison edge cases with empty strings
--FILE--
<?php
// == on empty strings, and the SIGN of the strncmp family. php's strncmp returns
// an unspecified-magnitude int ("< 0 / 0 / > 0"), and the magnitude even varies
// by memory layout between runs, so only the sign is portable -- PHL clamps to
// -1/0/1, php returns the raw byte difference.
echo ("" == "") ? "true" : "false", "
";
echo ("" == "a") ? "true" : "false", "
";
echo ("a" == "") ? "true" : "false", "
";
echo ("ab" == "ac") ? "true" : "false", "
";
echo ("a" == "a") ? "true" : "false", "
";
echo strncmp("", "", 0) <=> 0, "
";   // zero bytes -> equal
echo strncmp("", "a", 0) <=> 0, "
";  // zero bytes -> equal
echo strncmp("a", "", 0) <=> 0, "
";  // zero bytes -> equal
echo strncmp("ab", "ac", 2) <=> 0, "
"; // 'b' < 'c'
echo strncmp("ac", "ab", 2) <=> 0, "
"; // 'c' > 'b'

--EXPECT--
true
false
false
false
true
0
0
0
-1
1

--CLEAN--
<?php

