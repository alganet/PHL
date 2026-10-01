--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The unimplemented-abstract list is php's: link order, declaring class, capped at three
--DESCRIPTION--
Three things about that sentence were this engine's own. It walked its method
HASH, whose order is neither declaration order nor php's; it named the nearest
ancestor that HAD the method rather than the one that DECLARED it; and it listed
every method where php names at most three and writes ", ..." (the count in the
sentence staying the whole number).

php's order is the order it LINKS a class in -- the class's own declarations,
then inheritance, then the traits, then the interfaces, each of those
recursively -- with the abstract property HOOKS verified in a pass of their own
after the methods. So a trait's requirement comes ahead of an interface's though
the `use` is written inside the body and the `implements` in the header.

XDEBUG_MODE=off keeps the oracle's own module out of the report.
--ENV--
XDEBUG_MODE=off
--FILE--
<?php
abstract class AbslG { abstract function ga(); }
abstract class AbslP extends AbslG { abstract public int $h { get; } abstract function pa(); }
interface AbslI { function ia(); }
interface AbslJ extends AbslI { function ja(); }
trait AbslT { abstract function ta(); }
class AbslC extends AbslP implements AbslJ { use AbslT; }
echo "unreached";
--EXPECTF--
%AFatal error:  Class AbslC contains 6 abstract methods and must therefore be declared abstract or implement the remaining methods (AbslP::pa, AbslG::ga, AbslC::ta, ...) in %s on line 7%A
--CLEAN--
<?php
