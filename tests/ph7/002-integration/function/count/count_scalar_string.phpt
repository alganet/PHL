--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count() with string argument throws TypeError
--FILE--
<?php
count("hello");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: count(): Argument #1 ($value) must be of type Countable|array, %s given in %s
--CLEAN--
<?php

