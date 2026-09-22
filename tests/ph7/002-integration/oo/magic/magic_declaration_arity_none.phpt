--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A magic method php calls with NO arguments rejects any parameter, an optional one included
--DESCRIPTION--
`__destruct`, `__clone`, `__toString`, `__debugInfo`, `__serialize`, `__sleep`
and `__wakeup` are all called argumentless by the engine, and php gives their
row its own message rather than "must take exactly 0 arguments".

The parameter here is OPTIONAL, which is the interesting shape: it looks
harmless — every call site would still be valid — but php counts DECLARED
parameters, not required ones, so it is rejected exactly like `__destruct($a)`.
The mirror image is a variadic tail, which declares no parameter at all:
`__clone(...$a)` is accepted by both engines (pinned in
magic_declaration_accepted_shapes.phpt).
--FILE--
<?php
class Bad { public function __destruct($unused = null) {} }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__destruct() cannot take arguments in %s on line 2%A
