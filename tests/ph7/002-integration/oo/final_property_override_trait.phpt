--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait composed into a subclass may not supply a final property's name
--DESCRIPTION--
The trait is applied before the base is inherited, so its property is already in
the subclass's table when the final rule looks -- which is exactly how php sees
it too, and it words the refusal the same way as a direct redeclaration.
--FILE--
<?php
class FpotBase {
    final public int $p = 1;
}
trait FpotT { public int $p = 2; }
class FpotKid extends FpotBase {
    use FpotT;
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot override final property FpotBase::$p %s
--CLEAN--
<?php
