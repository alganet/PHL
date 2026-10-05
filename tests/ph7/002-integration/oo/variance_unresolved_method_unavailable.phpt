--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A method override naming classes nothing ever loads cannot be checked
--DESCRIPTION--
Where the declaration runs php autoloads what the check could not find, then
asks again; a class still missing is its own fatal, naming the first class
the check needed -- the parent's parameter type, the subtype side of a
contravariant pair.
--FILE--
<?php
class VuMuParent {
    function f(VuMuNowhereA $a): VuMuNowhereA {}
}
echo "before\n";
class VuMuKid extends VuMuParent {
    function f(VuMuNowhereB $a): VuMuNowhereC {}
}
echo "unreached\n";
?>
--EXPECTF--
before
%s Fatal error:  Could not check compatibility between VuMuKid::f(VuMuNowhereB $a): VuMuNowhereC and VuMuParent::f(VuMuNowhereA $a): VuMuNowhereA, because class VuMuNowhereA is not available in %s on line 7%A
