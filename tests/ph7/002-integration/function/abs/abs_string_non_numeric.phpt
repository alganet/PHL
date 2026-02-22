--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
abs("abc") should throw a TypeError with PHP-compatible message
--FILE--
<?php
abs("abc");
--EXPECTF--
%s Fatal error:  Uncaught TypeError: abs(): Argument #1 ($num) must be of type int|float, string given in %s
--CLEAN--
<?php

?>
