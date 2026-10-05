--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parameter typed with a class declared further down the file is checked once that class exists
--DESCRIPTION--
`VuPlDerived|int` over `VuPlBase` cannot be decided while the parent compiles:
neither class exists yet. php checks the pair again where the subclass is
declared -- by then both are, and `VuPlBase` is not under `VuPlDerived|int`.
--FILE--
<?php
class VuPlParent {
    function f(VuPlBase $a) {}
}
echo "before\n";
class VuPlKid extends VuPlParent {
    function f(VuPlDerived|int $a) {}
}
class VuPlBase {}
class VuPlDerived extends VuPlBase {}
echo "unreached\n";
?>
--EXPECTF--
before
%s Fatal error:  Declaration of VuPlKid::f(VuPlDerived|int $a) must be compatible with VuPlParent::f(VuPlBase $a) in %s on line 7%A
