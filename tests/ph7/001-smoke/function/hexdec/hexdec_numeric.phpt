--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec coerces a non-string scalar via its string form (PHP `string` ZPP)
--FILE--
<?php
// PHP renders a non-string scalar to its string form and hex-parses THAT,
// so hexdec(255) == hexdec("255") == 0x255 == 597 (not the decimal 255).
var_dump(hexdec(255));
var_dump(hexdec(16));
var_dump(hexdec(123456789));
var_dump(hexdec(true));
?>
--EXPECT--
int(597)
int(22)
int(4886718345)
int(1)
--CLEAN--
<?php
