--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert with non-empty string returns true (PHP 8 does not eval strings)
--FILE--
<?php
echo assert("1 == 0") ? "true" : "false";
?>
--EXPECT--
true
--CLEAN--
<?php

