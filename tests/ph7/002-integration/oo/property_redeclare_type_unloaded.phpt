--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared property typed with a class nothing loads must name the parent's class
--DESCRIPTION--
Neither class exists, so neither type can be resolved -- but property types
are invariant, and no class loaded later can be both under and over a
differently named one. php's unresolved obligation refuses the pair rather
than letting it through unchecked.
--FILE--
<?php
class PrdUlParent {
    public PrdUlNowhereA $p;
}
class PrdUlKid extends PrdUlParent {
    public PrdUlNowhereB $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PrdUlKid::$p must be PrdUlNowhereA (as in class PrdUlParent) in %s on line 5%A
