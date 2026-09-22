--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The destructor half of the no-return-type rule, and the type is rejected whatever it says
--DESCRIPTION--
The same rule as its constructor twin, on the method whose caller is the engine
itself. `never` is the interesting spelling: it describes a function that does
not return at all, so it reads like the one type a destructor could honestly
declare — php rejects it with everything else, because the rule is about the
DECLARATION carrying a type at all, not about which type it carries.
--FILE--
<?php
class Bad { public function __destruct(): never { throw new Exception("x"); } }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__destruct() cannot declare a return type in %s on line 2%A
