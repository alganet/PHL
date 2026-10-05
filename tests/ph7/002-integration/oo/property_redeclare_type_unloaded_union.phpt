--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared property may not drop an unloaded class from the parent's union
--DESCRIPTION--
The child's one class is the parent's own by name, so it is under the union;
but the union is over it only if the OTHER unloaded class were the same one,
which its different name rules out.
--FILE--
<?php
class PrdUuParent {
    public PrdUuNowhereA|PrdUuNowhereB $p;
}
class PrdUuKid extends PrdUuParent {
    public PrdUuNowhereA $p;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Type of PrdUuKid::$p must be PrdUuNowhereA|PrdUuNowhereB (as in class PrdUuParent) in %s on line 5%A
