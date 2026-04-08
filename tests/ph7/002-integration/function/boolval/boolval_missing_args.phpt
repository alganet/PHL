--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
boolval with no arguments should throw ArgumentCountError
--FILE--
<?php
boolval();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: boolval() expects exactly 1 argument, 0 given in %s
--CLEAN--
<?php

