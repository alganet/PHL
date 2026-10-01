--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exactly three unimplemented abstracts are listed whole, with no ", ..."
--DESCRIPTION--
The boundary of php's three-name cap, and the twin of abstract_gap_list_order:
the parent chain names the class that DECLARED each method (the grandparent, not
the parent that merely carries it), and the trait's requirement follows both.
--ENV--
XDEBUG_MODE=off
--FILE--
<?php
abstract class Absl3G { abstract function ga(); }
abstract class Absl3P extends Absl3G { abstract function pa(); }
trait Absl3T { abstract function ta(); }
class Absl3C extends Absl3P { use Absl3T; }
echo "unreached";
--EXPECTF--
%AFatal error:  Class Absl3C contains 3 abstract methods and must therefore be declared abstract or implement the remaining methods (Absl3P::pa, Absl3G::ga, Absl3C::ta) in %s on line 5%A
--CLEAN--
<?php
