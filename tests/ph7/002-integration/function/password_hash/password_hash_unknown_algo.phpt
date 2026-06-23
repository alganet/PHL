--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_hash with an unsupported algorithm throws a ValueError
--FILE--
<?php
password_hash("x", "nope");
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm%A
--CLEAN--
<?php
