--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hash_hmac() with an unknown algorithm throws a ValueError (PHP 8)
--FILE--
<?php
hash_hmac("nope", "data", "key");
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm%A
--CLEAN--
<?php
