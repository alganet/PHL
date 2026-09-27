--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A write through a SPECIALIZED builtin is php's refusal
--FILE--
<?php
// The wording says "built-in function" but the rule is php's opcode
// SPECIALIZATION: `strlen("x")` compiles to an opcode, so its result is a TMP
// and the write is refused. The branch meant to raise this had never fired --
// it asked GenStateCallBuiltinName for a name off the CALL node instead of the
// CALLEE node, so only `clone` ever reached it.
strlen("x")[0] = 1;
?>
--EXPECTF--
%ACannot use result of built-in function in write context%A
