--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared property adding null to an unloaded class type is refused
--DESCRIPTION--
No class could ever stand for null, so whether the class exists does not
matter: the child's type is wider than the parent's whatever it turns out
to be.
--FILE--
<?php
class PrdUnParent {
    public PrdUnNowhere $p;
}
class PrdUnKid extends PrdUnParent {
    public ?PrdUnNowhere $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PrdUnKid::$p must be PrdUnNowhere (as in class PrdUnParent) in %s on line 5%A
