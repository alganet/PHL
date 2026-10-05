--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A set-only interface property typed with a class nothing loads is refused where the class is declared
--DESCRIPTION--
The contravariant twin of the get-only case: a set-only property may only be
widened, which an unloaded class could still do, so php settles the pair at
the declaration and refuses it there with the supertype sentence.
--FILE--
<?php
interface VuSpIface {
    public VuSpNowhereA $p { set; }
}
echo "before\n";
class VuSpImpl implements VuSpIface {
    public VuSpNowhereB $p;
}
echo "unreached\n";
?>
--EXPECTF--
before
%s Fatal error:  Type of VuSpImpl::$p must be supertype of VuSpNowhereA (as in class VuSpIface) in %s on line 6%A
