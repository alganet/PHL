--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `void` return type is overridable by `void` and by `never`, and nothing else
--DESCRIPTION--
`void` is not under `mixed` the way every other type is, and `never` is under
everything -- the two ends of php's lattice, both of which the old comparator
skipped outright.
--FILE--
<?php
class OvvBase { public function h(): void {} }
class OvvC extends OvvBase { public function h(): int { return 1; } }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of OvvC::h(): int must be compatible with OvvBase::h(): void%A
--CLEAN--
<?php
