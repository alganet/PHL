--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_contains(array, ...) raises TypeError
--FILE--
<?php
str_contains(array(), "x");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: str_contains(): Argument #1 ($haystack) must be of type string, array given in %s
--CLEAN--
<?php

