--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff should throw TypeError when first argument is not an array
--FILE--
<?php
array_udiff("not an array", array(1), function($a, $b) { return $a - $b; });
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_udiff(): Argument #1 ($array) must be of type array, string given in %s
--CLEAN--
<?php

