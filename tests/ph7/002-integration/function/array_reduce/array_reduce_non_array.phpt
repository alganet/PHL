--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_reduce with non-array first argument throws TypeError
--FILE--
<?php
array_reduce('not_array', function($c, $i) { return $c; });
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_reduce(): Argument #1 ($array) must be of type array, %s given in %s
--CLEAN--
<?php

