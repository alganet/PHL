--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: non-int count raises TypeError
--FILE--
<?php
array_fill(0, 'foo', 'x');
?>
--EXPECTF--
PHP Fatal error:  Uncaught TypeError: array_fill(): Argument #2 ($count) must be of type int, string given in %s
--CLEAN--
<?php

