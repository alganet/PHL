--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce with one argument throws ArgumentCountError
--FILE--
<?php
array_reduce(array(1, 2, 3));
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_reduce() expects at least 2 arguments, 1 given in %s
--CLEAN--
<?php

