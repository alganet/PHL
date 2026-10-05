--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static property may not be redeclared as an instance one
--DESCRIPTION--
php checks static-ness before the type, so the static sentence wins even
when the types differ too.
--FILE--
<?php
class PrdStParent {
    public static int $p;
}
class PrdStKid extends PrdStParent {
    public string $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot redeclare static PrdStParent::$p as non static PrdStKid::$p %s
