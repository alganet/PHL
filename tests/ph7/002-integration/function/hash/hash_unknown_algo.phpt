--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash() with an unknown algorithm throws a ValueError (PHP 8)
--FILE--
<?php
hash("nope", "x");
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: hash(): Argument #1 ($algo) must be a valid hashing algorithm%A
--CLEAN--
<?php
