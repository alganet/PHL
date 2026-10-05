--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared property's type must be the parent's own type
--DESCRIPTION--
php's property types are INVARIANT: a child redeclaring `?int $p` as `int $p`
narrows it, which a plain property may not do even though every int is a
?int. Reported on the subclass's declaration line, naming the parent's type
as php renders it.
--FILE--
<?php
class PrdTiParent {
    public ?int $p;
}
class PrdTiKid
    extends PrdTiParent
{
    public int $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PrdTiKid::$p must be ?int (as in class PrdTiParent) in %s on line 5%A
