--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing an array to bin2hex should raise a TypeError
--FILE--
<?php
bin2hex(array());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: bin2hex(): Argument #1 ($string) must be of type string, array given in %s
--CLEAN--
<?php

