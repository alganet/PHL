--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A subclass may redeclare a base private attribute with no diagnostic
--FILE--
<?php
// php emits NO diagnostic when a child redeclares a base private property;
// the child instantiates and its own method reads its own value. (PHL cannot
// yet keep the base's shadowed same-named private as a distinct member — see
// NEWPLAN — so this checks only the child-visible value, which both engines
// agree on, plus the absence of a warning.)
class A {
    private $attr = 1;
}
class B extends A {
    private $attr = 2;
    public function bVal() { return $this->attr; }
}
$b = new B();
echo $b->bVal(), "\n";
echo "done\n";
?>
--EXPECT--
2
done
--CLEAN--
<?php
