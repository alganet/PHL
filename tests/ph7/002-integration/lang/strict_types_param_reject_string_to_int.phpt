--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types rejects a string passed to an int-typed parameter
--FILE--
<?php
declare(strict_types=1);
function f(int $x): int { return $x; }
f("1");
?>
--EXPECTF--
%ATypeError:%Af(): Argument #1 ($x) must be of type int, string given%A
