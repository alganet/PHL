--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_udiff should throw TypeError when callback is a string that does not resolve to a function
--FILE--
<?php
array_udiff(array(1), array(2), "not_a_function");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_udiff(): Argument #3 must be a valid callback, function "not_a_function" not found or invalid function name in %s
--CLEAN--
<?php

