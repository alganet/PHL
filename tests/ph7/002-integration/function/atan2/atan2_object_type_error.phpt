--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 object argument should raise TypeError
--FILE--
<?php
class MyObj {}
atan2(new MyObj, 1);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: atan2(): Argument #1 ($y) must be of type float, %s given in %s
--CLEAN--
<?php

