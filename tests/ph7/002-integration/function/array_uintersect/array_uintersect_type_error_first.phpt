--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should throw TypeError when the first argument is not an array
--FILE--
<?php
array_uintersect("not an array", array(1), function($a, $b) { return 0; });
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_uintersect(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

