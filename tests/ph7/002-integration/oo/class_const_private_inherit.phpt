--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Private class constant not inherited by child class
--FILE--
<?php
class ConstInhBase {
    private const PRIV = "base_private";
    protected const PROT = "base_protected";

    public function getPrivFromConstInhBase() {
        echo self::PRIV . "\n";
    }
}

class ConstInhChild extends ConstInhBase {
    public function getProtFromConstInhChild() {
        echo self::PROT . "\n";
    }

    public function getPrivFromConstInhChild() {
        echo parent::PRIV . "\n";
    }
}

$base = new ConstInhBase();
$base->getPrivFromConstInhBase();

$child = new ConstInhChild();
$child->getProtFromConstInhChild();
$child->getPrivFromConstInhChild();
echo "should not reach here\n";
?>
--EXPECTF--
base_private
base_protected
%s Fatal error:  Uncaught Error: Cannot access private constant ConstInhBase::PRIV in %s
--CLEAN--
<?php
unset($base, $child);
