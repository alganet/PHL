--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with an integer argument should raise TypeError
--FILE--
<?php
array_sum(42);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_sum(): Argument #1 ($array) must be of type array, %s given in %s
--CLEAN--
<?php

