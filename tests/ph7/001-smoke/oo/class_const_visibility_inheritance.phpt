--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class constant visibility with inheritance (protected accessible from child)
--FILE--
<?php
class ConstVisParent {
    public const PUB = "parent_public";
    protected const PROT = "parent_protected";

    public function getFromParent() {
        echo self::PUB . "\n";
        echo self::PROT . "\n";
    }
}

class ConstVisChild extends ConstVisParent {
    public function getFromChild() {
        echo parent::PUB . "\n";
        echo parent::PROT . "\n";
        echo self::PUB . "\n";
        echo self::PROT . "\n";
        echo static::PUB . "\n";
        echo static::PROT . "\n";
    }
}

$child = new ConstVisChild();
$child->getFromParent();
$child->getFromChild();
echo ConstVisChild::PUB . "\n";
?>
--EXPECT--
parent_public
parent_protected
parent_public
parent_protected
parent_public
parent_protected
parent_public
parent_protected
parent_public
--CLEAN--
<?php
unset($child);
