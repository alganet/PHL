--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A readonly property may not be redeclared without readonly
--DESCRIPTION--
php checks readonly-ness before the access level and the type, so this
sentence wins over the private redeclaration that follows it.
--FILE--
<?php
class PrdRoParent {
    protected readonly int $p;
}
class PrdRoKid extends PrdRoParent {
    private string $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot redeclare readonly property PrdRoParent::$p as non-readonly PrdRoKid::$p %s
