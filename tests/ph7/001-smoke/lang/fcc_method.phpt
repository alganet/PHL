--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
First-class callable of an instance method binds $this (and keeps it alive)
--FILE--
<?php
class FccM { public $v = 7; function add($x){ return $this->v + $x; } }
$o = new FccM();
$f = $o->add(...);
echo $f(3), "\n";
echo var_export($f instanceof Closure, true), "\n";
$o2 = new FccM(); $o2->v = 100;
$g = $o2->add(...);
$o2 = null;
echo $g(1), "\n";
echo implode(",", array_map($o->add(...), [10, 20])), "\n";
?>
--EXPECT--
10
true
101
17,27
--CLEAN--
<?php
