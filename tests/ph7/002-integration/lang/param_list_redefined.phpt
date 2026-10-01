--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter name written twice is a compile fatal, before any variadic complaint
--FILE--
<?php
class C { function m(...$x, $x) {} }
?>
--EXPECTF--
%s Fatal error:  Redefinition of parameter $x in %s
--CLEAN--
<?php
