--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strstr/stristr: the empty needle matches at position 0 (php 8)
--FILE--
<?php
// php 8 matches "" at position 0, so the whole haystack is returned.
var_dump(strstr("hello world", ""));
var_dump(stristr("hello world", ""));
// $before_needle then yields everything before position 0, i.e. nothing.
var_dump(strstr("hello world", "", true));
var_dump(strstr("", ""));
// A non-empty needle absent from the haystack still reports failure.
var_dump(strstr("hello", "z"));
?>
--EXPECT--
string(11) "hello world"
string(11) "hello world"
string(0) ""
string(0) ""
bool(false)
--CLEAN--
<?php
