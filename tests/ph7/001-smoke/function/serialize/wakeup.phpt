--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unserialize(): __wakeup / __unserialize hooks and exception propagation
--FILE--
<?php

class SzWake { public $a=1; function __wakeup(){ $this->a = 99; } }
echo unserialize(serialize(new SzWake))->a, "\n";
class SzUnser { public $x=0; function __serialize():array{ return ["x"=>$this->x]; } function __unserialize($d){ $this->x = $d["x"] + 1; } }
$o = new SzUnser; $o->x = 10;
echo unserialize(serialize($o))->x, "\n";
class SzThrow { function __wakeup(){ throw new \RuntimeException("boom"); } }
try { unserialize(serialize(new SzThrow)); echo "no-throw\n"; }
catch (\RuntimeException $e) { echo "caught:", $e->getMessage(), "\n"; }
?>
--EXPECT--
99
11
caught:boom
