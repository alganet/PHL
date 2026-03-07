--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values with no arguments throws ArgumentCountError
--FILE--
<?php
array_values();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_values() expects exactly 1 argument, 0 given in %s
--CLEAN--
<?php

