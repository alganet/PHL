--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_split with array argument triggers TypeError
--FILE--
<?php
str_split(array(1, 2, 3));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: str_split(): Argument #1 ($string) must be of type string, array given in %s
--CLEAN--
<?php

