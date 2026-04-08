--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert with non-empty array returns true
--FILE--
<?php
echo assert(array(1)) ? "true" : "false";
?>
--EXPECT--
true
--CLEAN--
<?php

