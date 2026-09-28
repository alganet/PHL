--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A VARIADIC parent signature still constrains the override
--DESCRIPTION--
The arity rule used to stand aside entirely the moment either signature was
variadic, so `f(string ...$b)` overridden by `f()` compiled here. php's rule is
that every call the parent accepts must reach the child: a variadic parent needs
a variadic child, the child may not DEMAND an argument the parent's callers do
not pass, and it must still accept every one they do.
--FILE--
<?php
class OvaBase { public function h(string ...$b): void {} }
class OvaC extends OvaBase { public function h(): void {} }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of OvaC::h(): void must be compatible with OvaBase::h(string ...$b): void%A
--CLEAN--
<?php
