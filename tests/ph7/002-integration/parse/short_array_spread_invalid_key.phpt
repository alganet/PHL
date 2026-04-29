--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: '[k => ...$a]' is rejected as a parse error
--FILE--
<?php
$a = [1, 2];
$b = [5 => ...$a];
?>
--EXPECTF--
PHP Parse error:  syntax error, unexpected token "..." in %s on line 3%A
--CLEAN--
<?php
