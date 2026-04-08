--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
intdiv with no arguments should throw ArgumentCountError
--FILE--
<?php
intdiv();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: intdiv() expects exactly 2 arguments, %d given in %s
--CLEAN--
<?php

