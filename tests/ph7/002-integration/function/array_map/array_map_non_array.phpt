--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with non-array second argument throws TypeError
--FILE--
<?php
array_map(function($v) { return $v; }, "string");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_map(): Argument #2 ($array) must be of type array, string given in %s
--CLEAN--
<?php

