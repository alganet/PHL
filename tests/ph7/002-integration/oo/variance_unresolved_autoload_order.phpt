--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The classes an unresolved override names are autoloaded in the order php checks them
--DESCRIPTION--
Parameters are checked before the return type, and within a pair the subtype
side first, so the autoloader is asked for VuAoA, VuAoB, then VuAoC. It
supplies only VuAoB; the pair is still open and names VuAoA.
--FILE--
<?php
spl_autoload_register(function ($c) {
    echo "load $c\n";
    if ($c === 'VuAoB') {
        eval('class VuAoB {}');
    }
});
class VuAoParent {
    function f(VuAoA $a): VuAoA {}
}
echo "before\n";
class VuAoKid extends VuAoParent {
    function f(VuAoB $a): VuAoC {}
}
echo "unreached\n";
?>
--EXPECTF--
before
load VuAoA
load VuAoB
load VuAoC
%s Fatal error:  Could not check compatibility between VuAoKid::f(VuAoB $a): VuAoC and VuAoParent::f(VuAoA $a): VuAoA, because class VuAoA is not available in %s on line 13%A
