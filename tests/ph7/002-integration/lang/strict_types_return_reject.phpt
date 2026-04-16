--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types rejects a string return from an :int-typed function
--FILE--
<?php
declare(strict_types=1);
function f(): int { return "1"; }
f();
?>
--EXPECTF--
%ATypeError:%Af(): Return value must be of type int, string returned%A
