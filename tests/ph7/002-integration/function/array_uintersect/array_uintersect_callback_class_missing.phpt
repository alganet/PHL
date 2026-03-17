--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should report missing class when callable is an array with 2 elements
--FILE--
<?php
array_uintersect(array(1), array(1), array("NopeClass", "method"));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_uintersect(): Argument #3 must be a valid callback, class "NopeClass" not found in %s
--CLEAN--
<?php

