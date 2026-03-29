--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self, static, and parent resolve correctly in ::, instanceof, and ::class
--FILE--
<?php
class Base {
    function selfClass() { return self::class; }
    function staticClass() { return static::class; }
    function isSelf() { return ($this instanceof self) ? "yes" : "no"; }
}

class Child extends Base {
    function parentClass() { return parent::class; }
    function childSelfClass() { return self::class; }
    function childStaticClass() { return static::class; }
    function isParent() { return ($this instanceof parent) ? "yes" : "no"; }
    function isStatic() { return ($this instanceof static) ? "yes" : "no"; }
}

$b = new Base();
echo $b->selfClass(), "\n";
echo $b->staticClass(), "\n";
echo $b->isSelf(), "\n";

$c = new Child();
echo $c->parentClass(), "\n";
echo $c->childSelfClass(), "\n";
echo $c->childStaticClass(), "\n";
echo $c->selfClass(), "\n";
echo $c->staticClass(), "\n";
echo $c->isParent(), "\n";
echo $c->isStatic(), "\n";
?>
--EXPECT--
Base
Base
yes
Base
Child
Child
Base
Child
yes
yes
--CLEAN--
<?php
