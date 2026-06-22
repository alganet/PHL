--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_equals() with a non-string user_string throws a TypeError naming argument #2
--FILE--
<?php
hash_equals("x", [1]);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: hash_equals(): Argument #2 ($user_string) must be of type string, array given%A
--CLEAN--
<?php
