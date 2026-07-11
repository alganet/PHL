--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
octdec coerces a non-string scalar via its string form (PHP `string` ZPP)
--FILE--
<?php
// PHP renders a non-string scalar to its string form and octal-parses THAT,
// so octdec(10) == octdec("10") == 8 (not the decimal 10).
var_dump(octdec(10));
var_dump(octdec(777));
var_dump(octdec("777"));
var_dump(octdec(true));
?>
--EXPECT--
int(8)
int(511)
int(511)
int(1)
--CLEAN--
<?php
