--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 with one argument should throw ArgumentCountError
--FILE--
<?php
atan2(1.0);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: atan2() expects exactly 2 arguments, 1 given in %s
--CLEAN--
<?php

