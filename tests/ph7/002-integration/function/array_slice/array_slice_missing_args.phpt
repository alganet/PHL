--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_slice with no arguments triggers ArgumentCountError
--FILE--
<?php
array_slice();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_slice() expects at least 2 arguments, 0 given in %s
--CLEAN--
<?php

