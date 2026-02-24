--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos string argument should raise TypeError
--FILE--
<?php
acos("foo");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: acos(): Argument #1 ($num) must be of type float, string given in %s
--CLEAN--
<?php

