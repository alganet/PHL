--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
floor() with no arguments should throw ArgumentCountError
--FILE--
<?php
floor();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: floor() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

