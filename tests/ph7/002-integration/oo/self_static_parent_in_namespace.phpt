--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self, static, parent work correctly inside namespaces
--FILE--
<?php
namespace App;

class Base {
    function selfClass() { return self::class; }
    function staticClass() { return static::class; }
}

class Child extends Base {
    function parentClass() { return parent::class; }
    function childSelf() { return self::class; }
    function isSelf() { return ($this instanceof self) ? "yes" : "no"; }
    function isParent() { return ($this instanceof parent) ? "yes" : "no"; }
}

$c = new Child();
echo $c->selfClass(), "\n";
echo $c->staticClass(), "\n";
echo $c->parentClass(), "\n";
echo $c->childSelf(), "\n";
echo $c->isSelf(), "\n";
echo $c->isParent(), "\n";
?>
--EXPECT--
App\Base
App\Child
App\Base
App\Child
yes
yes
--CLEAN--
<?php
