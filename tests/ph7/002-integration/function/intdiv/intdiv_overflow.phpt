--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
intdiv(PHP_INT_MIN, -1) should throw ArithmeticError
--FILE--
<?php
intdiv(-PHP_INT_MAX - 1, -1);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArithmeticError: Division of PHP_INT_MIN by -1 is not an integer in %s
--CLEAN--
<?php

