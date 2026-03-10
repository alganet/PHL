--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk with no arguments throws ArgumentCountError
--FILE--
<?php
array_walk();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_walk() expects at least 2 arguments, 0 given in %s
--CLEAN--
<?php

