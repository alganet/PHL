--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling array_slice with too many arguments triggers ArgumentCountError
--FILE--
<?php
array_slice(array(1), 0, 0, false, 'extra');
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_slice() expects at most 4 arguments, 5 given in %s
--CLEAN--
<?php

