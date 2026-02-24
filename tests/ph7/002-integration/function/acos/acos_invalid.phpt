--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos missing argument should throw ArgumentCountError
--FILE--
<?php
// Calling acos with no arguments should trigger a count error
acos();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: acos() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

