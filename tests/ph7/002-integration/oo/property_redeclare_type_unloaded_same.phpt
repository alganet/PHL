--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared property naming the same unloaded class is compatible
--DESCRIPTION--
The name alone answers the question, case-insensitively, and so does the
same nullable union spelled the other way round -- nothing has to be loaded.
--FILE--
<?php
class PrdUsParent {
    public PrdUsNowhere $a;
    public ?PrdUsNowhere $b;
    public PrdUsNowhere|int|null $c;
}
class PrdUsKid extends PrdUsParent {
    public prdusnowhere $a;
    public PrdUsNowhere|null $b;
    public null|int|PRDUSNOWHERE $c;
}
echo "ok\n";
?>
--EXPECT--
ok
