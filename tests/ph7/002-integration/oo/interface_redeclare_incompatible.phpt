--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A restated interface method must be compatible with the parent's
--DESCRIPTION--
php checks a restated interface method exactly as it checks an overriding class
method. PHL checked NEITHER parent -- an incompatible restatement of the first one
compiled in silence, and one of a later parent was refused for the wrong reason
("Cannot redeclare"), which is the declaration php ACCEPTS when the signatures agree.

Was a twin pair while php printed the two SIGNATURES in this sentence and PHL printed
the names alone. That is php's rendering now (F36), so the two halves say the same
thing and this is one test again.
--FILE--
<?php
interface IriA { public function f(): string; }
interface IriS { public function g(): string; }
interface IriB extends IriA, IriS { public function g(): int; }
echo "unreached\n";
?>
--EXPECTF--
%ADeclaration of IriB::g(): int must be compatible with IriS::g(): string%A
--CLEAN--
<?php
