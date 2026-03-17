--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should throw TypeError when the callback array has wrong arity
--FILE--
<?php
array_uintersect(array(1), array(1), array("foo"));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_uintersect(): Argument #3 must be a valid callback, array callback must have exactly two members in %s
--CLEAN--
<?php

