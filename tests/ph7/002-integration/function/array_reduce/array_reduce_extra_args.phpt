--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce with too many arguments throws ArgumentCountError
--FILE--
<?php
array_reduce(array(1), function($c, $i) { return $c; }, 0, 'extra');
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: array_reduce() expects at most 3 arguments, 4 given in %s
--CLEAN--
<?php

