--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An anonymous class's abstract-method refusal is raised when the `new` RUNS
--DESCRIPTION--
An anonymous class's declaration is an EXPRESSION, so php asks whether it leaves
an abstract method unimplemented at the `new` -- the refusal carries the frame
chain that reached it, the constructor arguments have not run yet, and a `new` on
a branch nothing takes is never asked at all. This engine mounts the class when
the unit compiles and used to ask there, so the whole FILE failed to load on a
`new` php never performs, and the fatal had only `#0 {main}` under it. A NAMED
class is unchanged: php refuses that one where the declaration stands.

XDEBUG_MODE=off keeps the oracle's own module out of the `Stack trace:` block.
--ENV--
XDEBUG_MODE=off
--FILE--
<?php
function anonArg($x) { echo "arg$x\n"; return $x; }
abstract class AnonAbs { public function __construct($a = null) {} abstract function z(); }
trait AnonT { function z() { return "t"; } }
// A `new` on a branch nothing takes is never checked at all.
function anonNever() { return new class extends AnonAbs {}; }
echo "loaded\n";
if (false) { anonNever(); }
echo "branch not taken\n";
// A class that DOES implement it is unaffected, however the method arrives.
echo (new class extends AnonAbs { use AnonT; })->z(), "\n";
echo (new class extends AnonAbs { function z() { return "i"; } })->z(), "\n";
// And the refusal, when the `new` runs: php's frame chain, and no constructor
// argument evaluated, because the declaration is refused before the arguments.
function anonBoom() { return new class(anonArg(1)) extends AnonAbs {}; }
function anonOuter() { return anonBoom(); }
anonOuter();
echo "unreached\n";
--EXPECTF--
loaded
branch not taken
t
i
%AFatal error:  Class AnonAbs@anonymous must implement 1 abstract method (AnonAbs::z) in %s on line 15
Stack trace:
#0 %s(16): anonBoom()
#1 %s(17): anonOuter()
#2 {main}%A
--CLEAN--
<?php
