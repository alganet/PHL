--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff should throw TypeError when an intermediate argument is not an array
--FILE--
<?php
array_udiff(array(1), "not an array", array(2), function($a, $b) { return $a - $b; });
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_udiff(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

