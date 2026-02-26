--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: missing arguments triggers ArgumentCountError
--FILE--
<?php
array_fill();
?>
--EXPECTF--
PHP Fatal error:  Uncaught ArgumentCountError: array_fill() expects exactly 3 arguments, 0 given in %s
--CLEAN--
<?php

