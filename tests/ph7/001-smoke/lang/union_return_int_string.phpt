--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union return type: : int|string is parsed and returned values pass through
--FILE--
<?php
function uris_r($flag): int|string {
    return $flag ? 42 : "hello";
}
$a = uris_r(true);
$b = uris_r(false);
echo is_int($a) ? "int:" : "str:", $a, "\n";
echo is_int($b) ? "int:" : "str:", $b, "\n";
?>
--EXPECT--
int:42
str:hello
--CLEAN--
<?php
