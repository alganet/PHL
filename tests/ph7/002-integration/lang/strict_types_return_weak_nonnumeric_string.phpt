--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Even in weak mode, a non-numeric string cannot coerce to :int on return
--FILE--
<?php
function f(): int { return "abc"; }
f();
?>
--EXPECTF--
%ATypeError:%Af(): Return value must be of type int, string returned%A
