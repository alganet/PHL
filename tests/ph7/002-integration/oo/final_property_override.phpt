--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A subclass may not redeclare a final property (PHP 8.4)
--DESCRIPTION--
php 8.4's final PROPERTY: the ban covers every spelling of the redeclaration --
a plain property, a static one, a promoted constructor parameter and a trait the
subclass composes -- because all four land in the subclass's attribute table
before inheritance runs. php names the class that DECLARED it, so a grandchild
still reads `FpoBase::$p`, and reports it on the subclass's declaration line.
--FILE--
<?php
class FpoBase {
    final public int $p = 1;
}
class FpoKid extends FpoBase {
    public int $p = 2;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot override final property FpoBase::$p %s
--CLEAN--
<?php
