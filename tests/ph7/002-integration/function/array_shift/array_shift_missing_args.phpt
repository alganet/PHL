--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_shift with no arguments triggers ArgumentCountError
--FILE--
<?php
array_shift();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_shift() expects exactly 1 argument, %d given in %s
