--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`parent` in a type declaration needs the class to HAVE one
--DESCRIPTION--
Each of the three scope keywords names something relative to where the
declaration is written, and php refuses one written where that thing does not
exist -- before the program runs, at the declaration itself. An interface has no
`parent` however many interfaces it extends; a trait defers the question to
whatever composes it, and a closure to whenever it is bound. The trait and the two
closures above the refusal are the accepting half: a compile fatal runs nothing, so
what they prove is that they COMPILED.
--FILE--
<?php
trait TskT { public function ok(parent $a) {} }
$defer = function (parent $a) {};
$arrow = fn (parent $a) => 1;
class TskA { public function f(parent $a) {} }
echo "unreachable\n";
?>
--EXPECTF--
%ACannot use "parent" when current class scope has no parent%A
--CLEAN--
<?php
