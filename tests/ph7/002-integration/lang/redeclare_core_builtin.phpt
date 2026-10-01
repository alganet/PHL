--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring a core C builtin is refused before the program runs at all
--FILE--
<?php
/* The redeclaration guard could only see the host functions registered while the
 * VM initialised. The ~650 core builtins registered AFTER the program compiled,
 * so `function strlen(){}` found an empty table, installed itself and won for the
 * rest of the run -- an accept-what-php-refuses and a wrong value at once. The
 * echo below is what proves WHEN the refusal lands: php binds a top-level
 * declaration at compile time, so not one statement of this file executes. */
echo "unreached-before\n";
function strlen($s) { return 42; }
echo "unreached-after\n";
--EXPECTF--
%AFatal error:%ACannot redeclare function strlen()%A
--CLEAN--
<?php
