--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_pop with no arguments triggers ArgumentCountError
--FILE--
<?php
array_pop();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_pop() expects exactly 1 argument, %d given in %s
--CLEAN--
<?php

