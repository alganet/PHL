--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert with string does not evaluate it (PHP 8 behavior)
--FILE--
<?php
echo assert("1 == 0") ? "OK" : "FAIL";
?>
--EXPECT--
OK
--CLEAN--
<?php

