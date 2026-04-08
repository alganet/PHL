--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
::class with self, static, and parent keywords
--FILE--
<?php
class CcspBase {
    function getSelf() { return self::class; }
    function getStatic() { return static::class; }
}
class CcspChild extends CcspBase {
    function getParent() { return parent::class; }
    function getChildSelf() { return self::class; }
    function getChildStatic() { return static::class; }
}
$b = new CcspBase();
$c = new CcspChild();
echo $b->getSelf() . "\n";
echo $b->getStatic() . "\n";
echo $c->getSelf() . "\n";
echo $c->getStatic() . "\n";
echo $c->getParent() . "\n";
echo $c->getChildSelf() . "\n";
echo $c->getChildStatic() . "\n";
?>
--EXPECT--
CcspBase
CcspBase
CcspBase
CcspChild
CcspBase
CcspChild
CcspChild
--CLEAN--
<?php
unset($b, $c);
