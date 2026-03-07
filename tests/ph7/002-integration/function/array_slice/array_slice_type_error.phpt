--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing a non-array to array_slice triggers TypeError
--FILE--
<?php
array_slice("hello", 1);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_slice(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

