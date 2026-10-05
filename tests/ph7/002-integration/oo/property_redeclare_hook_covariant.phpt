--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property over a get-only virtual parent may only narrow its type
--DESCRIPTION--
A virtual parent property with only a get hook is read and never written,
so its type is COVARIANT: the child may narrow `?int` to `int` but not widen
`int` to `?int`, and php says "subtype of".
--FILE--
<?php
abstract class PrdHcParent {
    abstract public int $p { get; }
}
class PrdHcKid extends PrdHcParent {
    public ?int $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PrdHcKid::$p must be subtype of int (as in class PrdHcParent) %s
