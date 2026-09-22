--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A magic method may not take its arguments BY REFERENCE
--DESCRIPTION--
The engine builds the arguments it passes to a magic method — a property name it
just read out of the opcode, an array it packed for `__call` — so there is no
caller variable behind them to write back to. php refuses the declaration
rather than leave the write-back silently going nowhere, which is what PHL did:
`__set($name, &$value)` compiled, and assigning to `$value` inside the body
changed nothing anywhere.

php checks exactly the arguments the arity row counts, which is why the check
runs only once the count already matches.
--FILE--
<?php
class Bad { public function __set($name, &$value) { $value = "written back"; } }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__set() cannot take arguments by reference in %s on line 2%A
