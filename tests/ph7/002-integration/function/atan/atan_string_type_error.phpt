--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan string argument should raise TypeError
--FILE--
<?php
atan("foo");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: atan(): Argument #1 ($num) must be of type float, string given in %s
--CLEAN--
<?php

