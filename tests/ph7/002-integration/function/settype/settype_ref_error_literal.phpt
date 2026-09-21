--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
settype() on a non-variable first argument throws a by-reference Error
--FILE--
<?php
settype(5, "int");
?>
--EXPECTF--
%s Fatal error:  Uncaught Error: settype(): Argument #1 ($var) could not be passed by reference in %s
