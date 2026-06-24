--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: extends + implements together
--FILE--
<?php
class AnonBaseB {}
interface AnonIfaceB {}
$o = new class(7) extends AnonBaseB implements AnonIfaceB {
    public $v;
    function __construct($v) { $this->v = $v; }
};
echo $o->v, " ", var_export($o instanceof AnonBaseB && $o instanceof AnonIfaceB, true), "\n";
?>
--EXPECT--
7 true
--CLEAN--
<?php
unset($o);
