--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Declaring an instance magic method `static` is a compile fatal
--DESCRIPTION--
Whether the engine has a receiver for a magic method is not the declaration's
to choose: `__get`, `__set`, `__isset`, `__unset`, `__call`, `__toString`,
`__invoke`, `__debugInfo`, `__serialize`, `__unserialize`, `__sleep`,
`__wakeup`, `__construct`, `__destruct` and `__clone` are all reached through an
OBJECT, so php refuses the `static` keyword on any of them.

PHL took the declaration at its word and dispatched the method anyway, so a
static `__get` ran with no `$this` — an accessor reading a per-instance table
found whatever the unbound scope happened to hold, in silence.

php checks this AFTER the arity, which is why `static function __get($a, $b)`
reports the count rather than this (pinned in magic_declaration_arity_one.phpt).
--FILE--
<?php
class Bad { public static function __get($name) { return 1; } }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__get() cannot be static in %s on line 2%A
