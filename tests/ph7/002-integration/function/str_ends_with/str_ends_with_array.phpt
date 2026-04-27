--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: str_ends_with(array, ...) raises TypeError
--FILE--
<?php
str_ends_with(array(), "x");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: str_ends_with(): Argument #1 ($haystack) must be of type string, array given in %s
--CLEAN--
<?php

