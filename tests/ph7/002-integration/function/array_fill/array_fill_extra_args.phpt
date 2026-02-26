--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: too many arguments triggers ArgumentCountError
--FILE--
<?php
array_fill(0,1,'x', 'extra');
?>
--EXPECTF--
PHP Fatal error:  Uncaught ArgumentCountError: array_fill() expects exactly 3 arguments, 4 given in %s
--CLEAN--
<?php

