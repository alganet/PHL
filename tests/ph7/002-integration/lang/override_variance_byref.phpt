--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An override must pass each parameter the same way the parent does
--DESCRIPTION--
php requires the by-reference-ness of every overlapping parameter to MATCH: a
child taking by value what the parent takes by reference silently loses the
caller's write. Nothing checked it here.
--FILE--
<?php
class OvbBase { public function h(int &$a): void {} }
class OvbC extends OvbBase { public function h(int $a): void {} }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of OvbC::h(int $a): void must be compatible with OvbBase::h(int &$a): void%A
--CLEAN--
<?php
