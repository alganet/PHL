--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strpos/stripos: the empty needle matches at the search offset (php 8)
--FILE--
<?php
// php 8 treats "" as matching at the offset, rather than as "not found".
var_dump(strpos("hello world", ""));
var_dump(stripos("hello world", ""));
var_dump(strpos("hello", "", 3));
// An offset equal to the length is still in range; the match is at the end.
var_dump(strpos("hello", "", 5));
// A negative offset counts back from the end.
var_dump(strpos("hello", "", -2));
// The empty haystack is not a special case.
var_dump(strpos("", ""));
?>
--EXPECT--
int(0)
int(0)
int(3)
int(5)
int(3)
int(0)
--CLEAN--
<?php
