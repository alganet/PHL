--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A get-only interface property typed with a class nothing loads is refused where the class is declared
--DESCRIPTION--
The implementing class narrows `VuGpNowhereA` to `VuGpNowhereB`, and neither
exists. A get-only property may be narrowed, so the pair is left open while
the file compiles and settled where the declaration runs -- after the line
above it has printed. Nothing loaded by then, the pair is refused.
--FILE--
<?php
interface VuGpIface {
    public VuGpNowhereA $p { get; }
}
echo "before\n";
class VuGpImpl implements VuGpIface {
    public VuGpNowhereB $p;
}
echo "unreached\n";
?>
--EXPECTF--
before
%s Fatal error:  Type of VuGpImpl::$p must be subtype of VuGpNowhereA (as in class VuGpIface) in %s on line 6%A
