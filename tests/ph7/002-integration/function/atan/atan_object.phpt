--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan object argument should raise TypeError
--FILE--
<?php
class MyObj {}
atan(new MyObj);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: atan(): Argument #1 ($num) must be of type float, %s given in %s
--CLEAN--
<?php

