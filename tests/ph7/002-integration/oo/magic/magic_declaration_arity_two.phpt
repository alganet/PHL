--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A magic method declared with the wrong argument COUNT is a compile fatal (__call, two required — plural wording)
--DESCRIPTION--
The plural half of php's arity wording, which is a separate message and not a
pluralized one: `must take exactly 2 arguments` for `__call`, `__callStatic`
and `__set`. A one-argument `__call` used to compile in PHL and then run with
its `$arguments` parameter unset on every catch-all dispatch.

A VARIADIC tail declares nothing for this count — php's `num_args` excludes it —
so `__call($name, ...$args)` is this same fatal, not an accepted spelling.
--FILE--
<?php
class Bad { public function __call($name) { return 1; } }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__call() must take exactly 2 arguments in %s on line 2%A
