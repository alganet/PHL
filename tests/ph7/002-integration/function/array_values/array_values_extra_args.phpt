--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values with too many arguments throws ArgumentCountError
--FILE--
<?php
array_values(array(1), 2);
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_values() expects exactly 1 argument, 2 given in %s
--CLEAN--
<?php

