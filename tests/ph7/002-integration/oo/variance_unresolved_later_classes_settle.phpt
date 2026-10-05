--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Classes declared further down the file answer an override's open variance pairs
--DESCRIPTION--
A get-only property narrowed and a method whose parameter widens and return
narrows, all naming classes the file declares later: each pair is settled
where its class is declared, by which point both classes exist. An anonymous
class settles the same way.
--FILE--
<?php
interface VuLsIface {
    public VuLsBase $p { get; }
}
class VuLsImpl implements VuLsIface {
    public VuLsDerived $p;
}
abstract class VuLsParent {
    abstract function f(VuLsDerived $a): VuLsBase;
}
class VuLsKid extends VuLsParent {
    function f(VuLsBase $a): VuLsDerived { return new VuLsDerived; }
}
$o = new class extends VuLsParent {
    function f(VuLsBase $a): VuLsDerived { return new VuLsDerived; }
};
class VuLsBase {}
class VuLsDerived extends VuLsBase {}
echo get_class((new VuLsKid)->f(new VuLsBase)), "\n";
echo get_class($o->f(new VuLsDerived)), "\n";
?>
--EXPECT--
VuLsDerived
VuLsDerived
