--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strrchr casts the needle to string and uses its first character (php 8)
--FILE--
<?php
// php 7 read a non-string needle as an ordinal; php 8 casts it to string, so
// 111 searches for "1" rather than for "o".
var_dump(strrchr("hello world", 111));
var_dump(strrchr("hello world1x", 111));
// Only the FIRST character of a multi-character needle is used.
var_dump(strrchr("hello world", "lo"));
// An empty needle matches nothing.
var_dump(strrchr("hello world", ""));
var_dump(strrchr("hello world", 0));
var_dump(strrchr("hello world", "z"));
?>
--EXPECT--
bool(false)
string(2) "1x"
string(2) "ld"
bool(false)
bool(false)
bool(false)
--CLEAN--
<?php
