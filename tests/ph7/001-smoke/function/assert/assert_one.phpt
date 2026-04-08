--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(1) returns true
--FILE--
<?php
echo assert(1) ? "true" : "false";
?>
--EXPECT--
true
--CLEAN--
<?php

