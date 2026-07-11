--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bindec coerces a non-string scalar via its string form (PHP `string` ZPP)
--FILE--
<?php
// PHP renders a non-string scalar to its string form and binary-parses THAT,
// so bindec(101) == bindec("101") == 5 (not the decimal 101).
var_dump(bindec(101));
var_dump(bindec(1010));
var_dump(bindec("1010"));
var_dump(bindec(true));
?>
--EXPECT--
int(5)
int(10)
int(10)
int(1)
--CLEAN--
<?php
