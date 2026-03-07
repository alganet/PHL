--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unique with too many arguments throws ArgumentCountError
--FILE--
<?php
array_unique(array(), 0, "extra");
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_unique() expects at most 2 arguments, 3 given in %s
--CLEAN--
<?php

