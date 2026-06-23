--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
password_hash with an out-of-range bcrypt cost throws a ValueError
--FILE--
<?php
password_hash("x", PASSWORD_BCRYPT, ["cost" => 3]);
?>
--EXPECTF--
%s Fatal error:  Uncaught ValueError: Invalid bcrypt cost parameter specified: 3%A
--CLEAN--
<?php
