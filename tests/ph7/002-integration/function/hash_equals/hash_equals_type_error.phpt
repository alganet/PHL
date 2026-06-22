--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_equals() with a non-string known_string throws a TypeError
--FILE--
<?php
hash_equals(123, "x");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: hash_equals(): Argument #1 ($known_string) must be of type string, int given%A
--CLEAN--
<?php
