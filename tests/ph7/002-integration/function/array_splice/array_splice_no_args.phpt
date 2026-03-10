--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with no arguments throws ArgumentCountError
--FILE--
<?php
array_splice();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_splice() expects at least 2 arguments, 0 given %s
--CLEAN--
<?php

