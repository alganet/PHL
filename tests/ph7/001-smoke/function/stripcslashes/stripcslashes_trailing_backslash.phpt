--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripcslashes keeps a backslash with nothing behind it
--FILE--
<?php
var_dump(stripcslashes('a\\'), stripcslashes('\\'), stripcslashes(''));
?>
--EXPECT--
string(2) "a\"
string(1) "\"
string(0) ""
--CLEAN--
<?php
