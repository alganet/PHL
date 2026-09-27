--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The specialization is per ARITY, so a wrong-arity call is a real call
--FILE--
<?php
// php gives up on the opcode when the argument shape does not fit, and a real
// call's result is writable through -- so this reaches the ArgumentCountError
// instead of the compile fatal.
strlen("x", 1)[0] = 1;
?>
--EXPECTF--
%AArgumentCountError%A
