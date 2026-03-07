--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values with non-array argument throws TypeError
--FILE--
<?php
array_values("string");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_values(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

