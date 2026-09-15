--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace ignores an empty search string but still consumes its replacement
--FILE--
<?php
// '' is ignored; 'l' must still pair with 'L', not with the skipped 'x'.
var_dump(str_replace(array('', 'l'), array('x', 'L'), 'hello'));
// The same alignment holds when the empty key sits in the middle.
var_dump(str_replace(array('a', '', 'b'), array('1', '2', '3'), 'ab'));
// A scalar replacement applies to every non-empty search string.
var_dump(str_replace(array('', 'l'), 'Z', 'hello'));
// A scalar empty search is a no-op.
var_dump(str_replace('', 'x', 'hello'));
?>
--EXPECT--
string(5) "heLLo"
string(2) "13"
string(5) "heZZo"
string(5) "hello"
--CLEAN--
<?php
