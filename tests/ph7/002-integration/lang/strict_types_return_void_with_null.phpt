--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A :void function must not return a value. PHP catches `return null;` at
compile time while PHL reports the same violation at runtime; both engines
error out with "void" in the message and a non-zero exit, so the pattern
accepts either wording.
--FILE--
<?php
function f(): void { return null; }
f();
?>
--EXPECTF--
%Avoid%A
