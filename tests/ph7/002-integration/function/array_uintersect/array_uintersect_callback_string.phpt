--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should throw TypeError when the callback is a non-existent function name
--FILE--
<?php
array_uintersect(array(1), array(1), "nope");
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_uintersect(): Argument #3 must be a valid callback, function "nope" not found or invalid function name in %s
--CLEAN--
<?php

