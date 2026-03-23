--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan too many arguments should throw ArgumentCountError
--FILE--
<?php
atan(1, 2);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: atan() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

