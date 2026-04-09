--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Protected member accessible from parent scope (bidirectional hierarchy)
--FILE--
<?php
class ProtParent {
    public function readChildProp(ProtChild $child) {
        echo $child->prot . "\n";
    }
}

class ProtChild extends ProtParent {
    protected $prot = "child_protected";
}

$parent = new ProtParent();
$child = new ProtChild();
$parent->readChildProp($child);
?>
--EXPECT--
child_protected
--CLEAN--
<?php
unset($parent, $child);
