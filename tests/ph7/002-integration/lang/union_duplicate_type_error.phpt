--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type: duplicate alternative is a fatal error
--FILE--
<?php
function f(int|int $x) {}
--EXPECTF--
PHP Fatal error:  Duplicate type int is redundant in %s on line %d%A
--CLEAN--
<?php
