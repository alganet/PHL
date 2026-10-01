--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A variadic parameter with a default value is a compile fatal
--FILE--
<?php
class C { function m($x, int ...$a = [1]) {} }
?>
--EXPECTF--
%s Fatal error:  Variadic parameter cannot have a default value in %s
--CLEAN--
<?php
