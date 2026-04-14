--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe method call on a real object invokes the method with arguments
--FILE--
<?php
class NsfMethodObjAdder {
    public function add($a, $b) { return $a + $b; }
}
$nsfMethodObj_x = new NsfMethodObjAdder();
echo $nsfMethodObj_x?->add(2, 3), "\n";
?>
--EXPECT--
5
--CLEAN--
<?php
unset($nsfMethodObj_x);
