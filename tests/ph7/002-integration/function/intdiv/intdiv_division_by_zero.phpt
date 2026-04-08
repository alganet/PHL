--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
intdiv(1, 0) should throw DivisionByZeroError
--FILE--
<?php
intdiv(1, 0);
?>
--EXPECTF--
%s Fatal error:  Uncaught DivisionByZeroError: Division by zero in %s
--CLEAN--
<?php

