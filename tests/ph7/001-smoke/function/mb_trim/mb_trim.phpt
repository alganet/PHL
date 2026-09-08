--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mb_trim/mb_ltrim/mb_rtrim strip whole multibyte chars, defaulting to Unicode whitespace
--FILE--
<?php
echo "[", mb_trim("  h\xc3\xa9llo  "), "][", mb_trim("xxhelloxx", "x"), "][", mb_ltrim("  hi"), "][", mb_rtrim("hi  "), "]\n";
echo "[", mb_trim("\xce\xb1\xce\xb1HELLO\xce\xb1\xce\xb1", "\xce\xb1"), "][", mb_trim("a-zHELLOa-z", "a..z"), "][", mb_trim("...hi...", "."), "]\n";
echo "[", mb_trim("\xe2\x80\x83\xe3\x80\x80 x \xc2\xa0"), "]\n";
echo "[", mb_trim("  hi  ", ""), "][", mb_trim(""), "][", mb_rtrim("h\xc3\xa9llo\xc2\xa0\xc2\xa0"), "]\n";
--EXPECT--
[héllo][hello][hi][hi]
[HELLO][-zHELLOa-][hi]
[x]
[  hi  ][][héllo]
--CLEAN--
<?php
