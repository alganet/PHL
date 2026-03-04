--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_sum with a non-array argument should raise TypeError
--FILE--
<?php
array_sum("not an array");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_sum(): Argument #1 ($array) must be of type array, %s given in %s
--CLEAN--
<?php

