--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
__construct() and __destruct() may not declare a return type at all
--DESCRIPTION--
The two magic methods that have no return VALUE. `new C` evaluates to the
instance, never to whatever the constructor returned, and `__destruct` is run by
the engine at a point with nowhere to put an answer — so php refuses any
declared type on either, `void` and `never` included, rather than let a
declaration promise something no caller can read.

PHL had no declaration rule and enforced the type at RUNTIME instead, so
`__construct(): int` raised a return TypeError at every instantiation: a
diagnostic on the CALL for a mistake in the declaration, and one that a
`catch (TypeError)` around the `new` could swallow.

`__clone` is deliberately NOT in this row — `public function __clone(): void {}`
is valid php, and stays accepted here (pinned in
magic_declaration_accepted_shapes.phpt).
--FILE--
<?php
class Bad { public function __construct(): void {} }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__construct() cannot declare a return type in %s on line 2%A
