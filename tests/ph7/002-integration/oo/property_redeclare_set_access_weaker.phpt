--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared readonly property may not narrow the protected(set) it implies
--DESCRIPTION--
A public readonly property is protected(set) without spelling it, so a subclass
may keep that set access or widen it, never make it private(set).
--FILE--
<?php
class PrdSwParent {
    public readonly int $p;
}
class PrdSwKid extends PrdSwParent {
    public private(set) readonly int $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Set access level of PrdSwKid::$p must be protected(set) (as in class PrdSwParent) or weaker %s
