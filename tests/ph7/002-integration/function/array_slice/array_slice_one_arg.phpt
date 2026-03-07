--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_slice with one argument triggers ArgumentCountError
--FILE--
<?php
array_slice(array(1, 2, 3));
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_slice() expects at least 2 arguments, 1 given in %s
--CLEAN--
<?php

