--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type: ?T cannot be combined with a |-union
--FILE--
<?php
function f(?int|string $x) {}
--EXPECTF--
PHP Parse error:  syntax error, unexpected token "|", expecting variable in %s on line %d%A
--CLEAN--
<?php
