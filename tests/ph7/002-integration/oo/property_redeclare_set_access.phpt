--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared property may not add a set visibility its public parent lacks
--DESCRIPTION--
The parent's set access is its read visibility, public here, so the subclass's
private(set) narrows it and php names the parent's as omitted.
--FILE--
<?php
class PrdSaParent {
    public int $p = 1;
}
class PrdSaKid extends PrdSaParent {
    public private(set) int $p = 2;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Set access level of PrdSaKid::$p must be omitted (as in class PrdSaParent) %s
