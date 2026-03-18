--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Passing an object without __toString to bin2hex should raise a TypeError
--FILE--
<?php
class Foo {}
bin2hex(new Foo());
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: bin2hex(): Argument #1 ($string) must be of type string, Foo given in %s
--CLEAN--
<?php

