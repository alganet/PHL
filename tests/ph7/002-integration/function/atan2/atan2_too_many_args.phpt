--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 too many arguments should throw ArgumentCountError
--FILE--
<?php
atan2(1, 2, 3);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: atan2() expects exactly 2 arguments, 3 given in %s
--CLEAN--
<?php

