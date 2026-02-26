--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: non-numeric string start index raises TypeError
--FILE--
<?php
array_fill('foo', 2, 'x');
?>
--EXPECTF--
PHP Fatal error:  Uncaught TypeError: array_fill(): Argument #1 ($start_index) must be of type int, string given in %s
--CLEAN--
<?php

