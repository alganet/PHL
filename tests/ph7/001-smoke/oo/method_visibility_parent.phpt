--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Protected method accessible via parent:: and static:: from child class
--FILE--
<?php
class MethVisBase {
    protected function protMethod() {
        return "protected_result";
    }

    protected static function protStatic() {
        return "protected_static_result";
    }
}

class MethVisChild extends MethVisBase {
    public function callViaParent() {
        echo parent::protMethod() . "\n";
        echo parent::protStatic() . "\n";
    }

    public function callViaStatic() {
        echo static::protStatic() . "\n";
    }
}

$child = new MethVisChild();
$child->callViaParent();
$child->callViaStatic();
?>
--EXPECT--
protected_result
protected_static_result
protected_static_result
--CLEAN--
<?php
unset($child);
