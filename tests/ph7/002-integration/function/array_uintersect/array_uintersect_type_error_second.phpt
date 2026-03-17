--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should throw TypeError when a non-array intermediate argument is passed
--FILE--
<?php
array_uintersect(array(1), "not an array", function($a, $b) { return 0; });
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_uintersect(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

