--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Foreach by reference over object properties and updating property values
--FILE--
<?php
class O { public $x; public function __construct(){ $this->x = 1; } }
$o = new O();
foreach ($o as $k => &$v) { $v++; echo $v . "\n"; }
echo $o->x . "\n";
?>
--EXPECT--
2
2

--CLEAN--
<?php
unset($o);
?>
