--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A promoted constructor parameter may not redeclare a final property
--DESCRIPTION--
A promoted parameter IS a property declaration, so the final rule reaches it.
--FILE--
<?php
class FpopBase {
    final public int $p = 1;
}
class FpopKid extends FpopBase {
    public function __construct(public int $p) {}
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot override final property FpopBase::$p %s
--CLEAN--
<?php
