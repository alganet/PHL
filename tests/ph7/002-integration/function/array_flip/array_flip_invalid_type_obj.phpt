--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip with an object argument should raise TypeError
--FILE--
<?php
class C{};
array_flip(new C());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_flip(): Argument #1 ($array) must be of type array, %s given in %s
--CLEAN--
<?php

