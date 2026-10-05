--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: a whole-valued float literal is a float to an int parameter, where a computed 4/2 is an int
--FILE--
<?php
echo "never printed\n";
function pdcrl_ok(int $x = 4 / 2) {}
function pdcrl_bad(int $x = -1.0) {}
--EXPECTF--
PHP Fatal error:  Cannot use float as default value for parameter $x of type int in %s on line 4%A
--CLEAN--
<?php
