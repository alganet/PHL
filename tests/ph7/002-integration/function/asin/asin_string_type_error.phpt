--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asin string argument should raise TypeError
--FILE--
<?php
asin("foo");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: asin(): Argument #1 ($num) must be of type float, string given in %s
--CLEAN--
<?php

