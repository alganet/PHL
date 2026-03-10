--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() with null argument throws TypeError
--FILE--
<?php
count(null);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: count(): Argument #1 ($value) must be of type Countable|array, %s given in %s
--CLEAN--
<?php

