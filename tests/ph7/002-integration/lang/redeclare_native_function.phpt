--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring a builtin that is implemented in C is a fatal error, like a prelude one
--FILE--
<?php
/* A function that moves from an embedded PHP chunk into a C routine moves from
 * the compiled-function table into the host-function table, and the
 * redeclaration guard used to consult only the former -- so converting a builtin
 * to C silently made it shadowable. `function ini_get(){}` then WON over the
 * builtin for the rest of the program, where php fatals. The guard now checks
 * both tables for anything registered before user code compiles. */
function ini_get() {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare function ini_get()%A
--CLEAN--
<?php
