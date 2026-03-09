--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with one argument throws ArgumentCountError
--FILE--
<?php
array_map(function($v) { return $v; });
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_map() expects at least 2 arguments, 1 given in %s
--CLEAN--
<?php

