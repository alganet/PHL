--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing a string offset to array_slice triggers TypeError
--FILE--
<?php
array_slice(array(1), "hello");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_slice(): Argument #2 ($offset) must be of type int, string given in %s
--CLEAN--
<?php

