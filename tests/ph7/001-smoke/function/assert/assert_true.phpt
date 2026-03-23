--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(true) returns true
--FILE--
<?php
echo assert(true) ? "OK" : "FAIL";
?>
--EXPECT--
OK
--CLEAN--
<?php

